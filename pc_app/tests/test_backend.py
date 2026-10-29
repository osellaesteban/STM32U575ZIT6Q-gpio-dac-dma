"""Tests of the parts of the application that don't need a window.

Run from pc_app/:  python -m unittest discover -s tests -v
"""
import csv
import json
import os
import tempfile
import time
import unittest
from pathlib import Path

os.environ.setdefault("QT_QPA_PLATFORM", "offscreen")

from stimmux import fwproto, runner, units
from stimmux.model import (CHANNELS, ChannelSetup, Protocol, ProtocolBlock, Session, StimConfig, Subject,
                           load_json, save_json)
from stimmux.simulator import SIM_BASELINE, SimulatedBoard
from stimmux.storage import SessionRecorder, export_hdf5, load_session, read_events


def enabled_config(limit=20.0, working=10.0) -> StimConfig:
    config = StimConfig()
    for ch in config.channels[:2]:
        ch.enabled = True
        ch.safety_limit_ma = limit
        ch.working_level_ma = working
    return config


class ModelTests(unittest.TestCase):
    def test_round_trip(self):
        config = enabled_config()
        config.protocol.blocks = [ProtocolBlock("ramp", 2.0, "percent", [0.0] * 8, [100.0] * 8)]
        session = Session(subject=Subject(code="S012", birth_year=1990), config=config)
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "s.json"
            save_json(path, "session", session)
            loaded = load_json(path, Session, "session")
        self.assertEqual(loaded, session)

    def test_tolerant_loading(self):
        data = {"schema": 1, "kind": "stimcfg", "data": {
            "channels": [{"index": 3, "enabled": True, "unknown_field": 1}],
            "protocol": {"blocks": [{"kind": "hold", "start": [5]}]},
            "future": {"x": 1}}}
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "c.json"
            path.write_text(json.dumps(data))
            config = load_json(path, StimConfig, "stimcfg")
            self.assertEqual(len(config.channels), CHANNELS)
            self.assertTrue(config.channels[3].enabled)
            self.assertEqual(config.channels[0].label, "CH0")
            self.assertEqual(config.protocol.blocks[0].start, [5, 0, 0, 0, 0, 0, 0, 0])
            with self.assertRaises(ValueError):
                load_json(path, Session, "session")

    def test_firmware_label(self):
        self.assertEqual(ChannelSetup(index=2, label="Tib ant der (largo)").firmware_label(), "Tib_ant_der__la")
        self.assertEqual(ChannelSetup(index=5).firmware_label(), "CH5")


class UnitsTests(unittest.TestCase):
    def test_conversions(self):
        config = StimConfig()
        config.ds8r.range_ma = 50
        self.assertEqual(units.ma_to_permille(25, config.ds8r), 500)
        self.assertEqual(units.ma_to_permille(80, config.ds8r), 1000)
        self.assertEqual(units.ma_to_permille(-1, config.ds8r), 0)
        self.assertAlmostEqual(units.permille_to_ma(500, config.ds8r), 25)
        self.assertIsNone(units.counts_to_ma(1000, 100, 0))
        self.assertAlmostEqual(units.counts_to_ma(1100, 100, 0.01), 10.0)

    def test_channel_max(self):
        config = enabled_config(limit=30)
        config.ds8r.range_ma = 25
        self.assertEqual(units.channel_max_ma(config.channels[0], config.ds8r), 25)
        self.assertEqual(units.channel_max_ma(config.channels[5], config.ds8r), 0)

    def test_timing_rules(self):
        config = StimConfig()
        self.assertEqual(units.check_timing(config.timing, config.ds8r), [])
        config.timing.period_us = 6000              # 8 slots of 750 us
        self.assertTrue(units.check_timing(config.timing, config.ds8r))
        config.timing.period_us = 25000
        config.ds8r.pulse_width_us = 3000
        self.assertTrue(units.check_timing(config.timing, config.ds8r))

    def test_channel_rules(self):
        config = enabled_config()
        self.assertEqual(units.check_channels(config), [])
        config.channels[0].safety_limit_ma = 0
        config.channels[1].working_level_ma = 50
        self.assertEqual(len(units.check_channels(config)), 2)


class ProtocolLineTests(unittest.TestCase):
    def test_parse_pulse(self):
        r = fwproto.parse_pulse("P 120 1 3 150 2048 - 50 2900 2850 2860 2700 + 49 2890 2845 2851 2702")
        self.assertEqual((r.frame, r.slot, r.channel, r.amplitude, r.baseline), (120, 1, 3, 150, 2048))
        self.assertEqual(len(r.phases), 2)
        self.assertFalse(r.phases[0].positive)
        self.assertEqual(r.phases[1].min, 2702)
        self.assertEqual(len(fwproto.parse_pulse("P 1 0 0 10 100 + 5 9 8 7 6").phases), 1)
        for bad in ("P 1 2", "P 1 0 0 10 100 x 5 9 8 7 6", "PING", "P a 0 0 10 100 + 5 9 8 7 6"):
            self.assertIsNone(fwproto.parse_pulse(bad))

    def test_wave_assembler(self):
        a = fwproto.WaveAssembler()
        self.assertIsNone(a.feed("WH 3 0 1 500 + 0 2 4 1 2 0 3"))
        self.assertIsNone(a.feed("WD 0 10 20"))
        self.assertIsNone(a.feed("WD 2 30 40"))
        wave = a.feed("WE")
        self.assertEqual(wave.samples, [10, 20, 30, 40])
        self.assertEqual((wave.channel, wave.i_phase1, wave.i_end), (1, 2, 3))
        # missing data line -> discarded
        a.feed("WH 3 0 1 500 + 0 2 4 1 2 0 3")
        a.feed("WD 2 30 40")
        self.assertIsNone(a.feed("WE"))

    def test_config_commands_accepted_by_simulator(self):
        board = SimulatedBoard(seed=1)
        config = enabled_config()
        config.timing.sequence = [0, 1, 0, 1]
        config.channels[1].polarity = "BIPH"
        for command in fwproto.config_commands(config):
            self.assertEqual(board.handle(command), "OK", command)
        self.assertEqual(board.timing.sequence, [0, 1, 0, 1])
        self.assertEqual(board.pol[1], "BIPH")


class RunnerTests(unittest.TestCase):
    def test_expand_and_amplitudes(self):
        config = enabled_config(limit=20, working=10)
        protocol = Protocol(blocks=[
            ProtocolBlock("ramp", 2.0, "percent", [0.0] * 8, [100.0, 50.0] + [0.0] * 6),
            ProtocolBlock("hold", 1.0, "ma", [12.0, 3.0] + [0.0] * 6),
            ProtocolBlock("rest", 0.5),
            ProtocolBlock("repeat", repeat_span=2, repeat_count=2),      # hold+rest twice more
            ProtocolBlock("repeat", repeat_span=1, repeat_count=1),      # the previous repeat once more
        ])
        self.assertEqual(runner.validate(protocol, config), [])
        segments = runner.expand(protocol, config.channels)
        self.assertEqual([s.kind for s in segments],
                         ["ramp", "hold", "rest"] + ["hold", "rest"] * 2 + ["hold", "rest"] * 2)
        self.assertAlmostEqual(runner.total_duration(segments), 2 + 1.5 * 5)
        run = runner.ProtocolRun(segments)
        amps, index = run.at(1.0)
        self.assertEqual(index, 0)
        self.assertAlmostEqual(amps[0], 5.0)          # half of a 0 -> 10 mA ramp
        self.assertAlmostEqual(amps[1], 2.5)
        amps, index = run.at(2.5)
        self.assertEqual((index, amps[0]), (1, 12.0))
        amps, index = run.at(3.2)
        self.assertEqual((index, amps[0]), (2, 0.0))
        self.assertFalse(run.finished(9.4))
        self.assertTrue(run.finished(9.5))
        self.assertEqual(run.at(10)[0], [0.0] * 8)

    def test_validation(self):
        config = enabled_config(limit=10, working=8)
        too_high = Protocol(blocks=[ProtocolBlock("hold", 1.0, "percent", [150.0] + [0.0] * 7)])
        self.assertIn("supera el límite", runner.validate(too_high, config)[0])
        disabled = Protocol(blocks=[ProtocolBlock("hold", 1.0, "ma", [0, 0, 1.0] + [0.0] * 5)])
        self.assertIn("deshabilitado", runner.validate(disabled, config)[0])
        bad_repeat = Protocol(blocks=[ProtocolBlock("repeat", repeat_span=1, repeat_count=1)])
        self.assertTrue(runner.validate(bad_repeat, config))
        self.assertTrue(runner.validate(Protocol(), config))
        config.ds8r.range_ma = 5      # the DS8R range also limits
        ok = Protocol(blocks=[ProtocolBlock("hold", 1.0, "ma", [6.0] + [0.0] * 7)])
        self.assertTrue(runner.validate(ok, config))


class SimulatorTests(unittest.TestCase):
    def setUp(self):
        self.board = SimulatedBoard(seed=2)
        self.t = 100.0
        self.board.pending.clear()

    def cmd(self, line):
        return self.board.handle(line, self.t)

    def test_run_reports_and_done(self):
        for line in ("SEQ 0 1", "PERIOD 4000", "ENS 1 1", "AMPS 100 200", "CH 1 POL ALT", "MON 1"):
            self.assertEqual(self.cmd(line), "OK")
        self.assertEqual(self.cmd("RUN 3"), "OK")
        self.assertEqual(self.cmd("PERIOD 5000"), "ERR BUSY")
        out = self.board.tick(self.t + 1.0)
        pulses = [fwproto.parse_pulse(l) for l in out if l.startswith("P ")]
        self.assertEqual(len(pulses), 6)
        self.assertEqual([p.channel for p in pulses], [0, 1] * 3)
        self.assertEqual([p.phases[0].positive for p in pulses if p.channel == 1], [True, False, True])
        self.assertIn("EVT DONE FRAMES=3", out)
        self.assertGreater(pulses[1].phases[0].mean, pulses[0].phases[0].mean)
        self.assertEqual(pulses[0].baseline, SIM_BASELINE)

    def test_watchdog_and_wave(self):
        for line in ("SEQ 0", "PERIOD 2000", "ENS 1", "AMPS 300", "WDOG 100"):
            self.assertEqual(self.cmd(line), "OK")
        self.assertEqual(self.cmd("RUN"), "OK")
        self.assertEqual(self.cmd("WAVE 0"), "OK")
        self.assertEqual(self.cmd("WAVE"), "ERR BUSY")
        out = self.board.tick(self.t + 0.05)
        assembler = fwproto.WaveAssembler()
        waves = [w for w in (assembler.feed(l) for l in out) if w]
        self.assertEqual(len(waves), 1)
        self.assertEqual(waves[0].n, len(waves[0].samples))
        self.assertIn("EVT WDOG", self.board.tick(self.t + 0.5))
        self.assertFalse(self.board.running)

    def test_errors(self):
        self.assertEqual(self.cmd("CH 9 AMP 1"), "ERR CHANNEL")
        self.assertEqual(self.cmd("AMPS 2000"), "ERR RANGE")
        self.assertEqual(self.cmd("PERIOD 6000"), "OK")
        self.assertEqual(self.cmd("RUN"), "ERR TIMING")
        self.assertEqual(self.cmd("XYZ"), "ERR UNKNOWN")


class StorageTests(unittest.TestCase):
    def test_record_load_export(self):
        config = enabled_config()
        config.calibration.ma_per_count = 0.01
        config.timing.period_us = 10000
        config.timing.sequence = [0, 1]
        session = Session(subject=Subject(code="S 012/x"), config=config)
        session.experiment.protocol_name = "marcha"
        with tempfile.TemporaryDirectory() as tmp:
            rec = SessionRecorder(Path(tmp), session)
            self.assertIn("S-012-x_marcha", rec.folder.name)
            rec.start_run()
            now = time.monotonic()
            rec.pulse(fwproto.parse_pulse("P 2 1 1 500 100 + 50 1200 1100 1100 1000"), now)
            rec.pulse(fwproto.parse_pulse("P 2 0 0 500 100 + 25 900 800 800 700 - 25 905 805 801 702"), now)
            rec.event("marker", "fatiga")
            board = SimulatedBoard(seed=3)
            board.handle("SEQ 0")
            board.handle("PERIOD 2000")
            board.handle("ENS 1")
            board.handle("AMPS 100")
            board.handle("RUN", 0.0)
            board.handle("WAVE", 0.0)
            assembler = fwproto.WaveAssembler()
            wave = [w for w in (assembler.feed(l) for l in board.tick(0.01)) if w][0]
            wave_path = rec.wave(wave, now)
            rec.close()
            folder = rec.folder

            loaded = load_session(folder)
            self.assertEqual(loaded.subject.code, "S 012/x")
            self.assertTrue(loaded.closed)
            with open(folder / "pulses.csv", newline="", encoding="utf-8") as f:
                rows = list(csv.DictReader(f))
            self.assertEqual(len(rows), 2)
            self.assertEqual(rows[0]["t_run_s"], "0.025000")      # frame 2 * 10 ms + slot 1 * 5 ms
            self.assertEqual(rows[0]["p1_mean_ma"], "10.0000")    # (1100 - 100) * 0.01
            self.assertEqual(rows[0]["p2_pol"], "")
            self.assertEqual(rows[1]["p2_pol"], "-")
            kinds = [e["kind"] for e in read_events(folder)]
            self.assertEqual(kinds, ["session", "marker", "wave", "session"])
            self.assertTrue(wave_path.exists())

            import h5py
            h5_path = export_hdf5(folder)
            with h5py.File(h5_path) as h5:
                self.assertEqual(json.loads(h5.attrs["session_json"])["kind"], "session")
                self.assertEqual(h5["pulses"].shape, (2,))
                self.assertAlmostEqual(float(h5["pulses"]["p1_mean_ma"][0]), 10.0)
                self.assertEqual(h5["events"].shape, (4, 4))
                self.assertEqual(h5["waves/wave_0001"].shape[0], wave.n)


class DeviceTests(unittest.TestCase):
    """Device + simulated transport in a Qt event loop."""

    def test_connect_configure_run(self):
        from PySide6.QtWidgets import QApplication
        from stimmux.device import Device
        from stimmux.transport import SIMULATOR_PORT

        app = QApplication.instance() or QApplication([])     # shared with test_ui
        device = Device()
        pulses, events, info = [], [], {}
        device.pulseReceived.connect(lambda r, t: pulses.append(r))
        device.eventReceived.connect(events.append)
        device.connect_port(SIMULATOR_PORT, info.update)
        config = enabled_config()
        config.timing.sequence = [0, 1]
        config.timing.period_us = 4000
        errors = [None]
        device.send_all(fwproto.config_commands(config) + ["ENS 1 1", "AMPS 50 60", "MON 1", "RUN 5"],
                        lambda e: errors.__setitem__(0, e))

        deadline = time.monotonic() + 3
        while time.monotonic() < deadline and not any(e.startswith("EVT DONE") for e in events):
            app.processEvents()
            time.sleep(0.005)
        device.disconnect_port()
        self.assertEqual(info.get("FW"), "0.3")
        self.assertEqual(errors[0], [])
        self.assertEqual(len(pulses), 10)
        self.assertIn("EVT DONE FRAMES=5", events)


if __name__ == "__main__":
    unittest.main()
