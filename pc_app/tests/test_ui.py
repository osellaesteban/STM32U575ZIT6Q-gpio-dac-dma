"""End to end test of the main window with the simulated board (offscreen).

Dialogs are answered automatically ("Yes"); the test drives the same
controller methods the buttons call.
"""
import csv
import os
import tempfile
import time
import unittest
from pathlib import Path

os.environ.setdefault("QT_QPA_PLATFORM", "offscreen")
os.environ["STIMMUX_SETTINGS"] = "StimMuxTest"     # don't touch the user settings

from PySide6.QtWidgets import QApplication, QMessageBox

from stimmux.storage import export_hdf5, load_session, read_events
from stimmux.transport import SIMULATOR_PORT
from stimmux.ui.main_window import MainWindow
from stimmux.ui.stimulation import parse_values


def wait(app, seconds, until=None):
    deadline = time.monotonic() + seconds
    while time.monotonic() < deadline:
        app.processEvents()
        if until is not None and until():
            return True
        time.sleep(0.005)
    return until() if until is not None else True


class UiTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.app = QApplication.instance() or QApplication([])
        cls.messages = []
        answer = lambda *args, **kwargs: (cls.messages.append(args[2] if len(args) > 2 else ""),
                                          QMessageBox.Yes)[1]
        for name in ("question", "warning", "information", "critical"):
            setattr(QMessageBox, name, staticmethod(answer))

    def test_parse_values(self):
        self.assertEqual(parse_values("10"), [10.0] * 8)
        self.assertEqual(parse_values("20; 35,5; 0"), [20.0, 35.5] + [0.0] * 6)
        self.assertEqual(parse_values("1 2"), [1.0, 2.0] + [0.0] * 6)
        with self.assertRaises(ValueError):
            parse_values("1;2;3;4;5;6;7;8;9")

    def test_full_session(self):
        with tempfile.TemporaryDirectory() as tmp:
            window = MainWindow()
            window.settings.setValue("data_root", tmp)
            ctrl = window.ctrl
            app = self.app

            # connection
            window.ports.setCurrentIndex(window.ports.findData(SIMULATOR_PORT))
            window._toggle_connection()
            self.assertTrue(wait(app, 2, lambda: ctrl.device.info.get("FW") == "0.3"))

            # stimulation needs a session
            self.assertFalse(ctrl.start_manual())

            # subject / experiment / channels through the model the forms edit
            session = ctrl.session
            session.subject.code = "S012"
            session.experiment.protocol_name = "prueba"
            session.experiment.consent_obtained = True
            session.config.calibration.ma_per_count = 0.01
            session.config.timing.sequence = [0, 1]
            session.config.timing.period_us = 10000
            for ch in session.config.channels[:2]:
                ch.enabled = True
                ch.safety_limit_ma = 20
                ch.working_level_ma = 10
            session.config.channels[1].polarity = "ALT"
            window._refresh_all()
            window._toggle_session()
            self.assertTrue(ctrl.session_open)
            folder = ctrl.recorder.folder
            self.assertTrue(str(folder).startswith(tmp))

            # thresholds on channel 0
            self.assertTrue(ctrl.start_threshold(0, 1.0, 1.0, 0.2, 15.0))
            self.assertTrue(wait(app, 3, lambda: ctrl.threshold_level() >= 3.0))
            self.assertIsNotNone(ctrl.mark_threshold("sensory"))
            self.assertTrue(wait(app, 3, lambda: ctrl.threshold_level() >= 5.0))
            level = ctrl.mark_threshold("max")
            self.assertEqual(ctrl.state, "idle")
            self.assertEqual(session.config.channels[0].threshold_max_ma, round(level, 3))

            # manual: amplitudes are clamped to the limit
            window.tabs.setCurrentWidget(window.stimulation_tab)
            window.stimulation_tab.manual.spins[0].setValue(5.0)
            ctrl.set_manual_amplitude(1, 99.0)
            self.assertTrue(window.confirm_start("control manual"))
            self.assertTrue(ctrl.start_manual())
            self.assertTrue(wait(app, 2, lambda: ctrl.state == "manual" and ctrl._keepalive.isActive()))
            self.assertTrue(wait(app, 1, lambda: ctrl._sent_permille[0] == 50))
            self.assertEqual(ctrl._sent_permille[1], 200)          # 20 mA limit of 100 mA range
            wait(app, 0.4)
            ctrl.stop()
            self.assertEqual(ctrl.state, "idle")

            # protocol: ramp to 100 % of the working level and hold
            window.stimulation_tab.protocol._add()
            window.stimulation_tab.protocol._add()
            blocks = session.config.protocol.blocks
            blocks[0].kind, blocks[0].duration_s = "ramp", 0.3
            blocks[0].start, blocks[0].end = [0.0] * 8, [100.0, 50.0] + [0.0] * 6
            blocks[1].duration_s = 0.2
            blocks[1].start = [100.0, 50.0] + [0.0] * 6
            session.config.protocol.name = "rampa"
            self.assertTrue(ctrl.start_protocol())
            self.assertTrue(wait(app, 3, lambda: ctrl.state == "protocol"))
            self.assertTrue(wait(app, 3, lambda: ctrl.state == "idle"))
            self.assertIn("Protocolo terminado", [e["text"] for e in read_events(folder)])

            # waveform
            ctrl.start_manual()
            wait(app, 0.3)
            ctrl.request_wave(1)
            self.assertTrue(wait(app, 2, lambda: ctrl.recorder.wave_count == 1))
            ctrl.stop()

            window.marker.setText("fatiga")
            window._add_marker()
            window._toggle_session()
            self.assertFalse(ctrl.session_open)
            window.close()

            loaded = load_session(folder)
            self.assertEqual(loaded.subject.code, "S012")
            self.assertIsNotNone(loaded.config.channels[0].threshold_sensory_ma)
            with open(folder / "pulses.csv", newline="", encoding="utf-8") as f:
                pulses = list(csv.DictReader(f))
            self.assertGreater(len(pulses), 20)
            self.assertEqual({p["run"] for p in pulses}, {"1", "2", "3", "4"})
            self.assertTrue(all(p["p1_mean_ma"] for p in pulses))
            kinds = [e["kind"] for e in read_events(folder)]
            for kind in ("run", "threshold", "stop", "marker", "wave"):
                self.assertIn(kind, kinds)
            self.assertTrue((folder / "waves" / "wave_0001.csv").exists())
            self.assertIn("RUN", (folder / "commands.log").read_text(encoding="utf-8"))
            export_hdf5(folder)
            window.settings.clear()


if __name__ == "__main__":
    unittest.main()
