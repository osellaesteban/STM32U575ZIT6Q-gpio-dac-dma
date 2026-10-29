"""Session folder: recording, loading and HDF5 export.

<root>/<YYYY-MM-DD_HHMMSS>_<subject>_<protocol>/
    session.json    subject, experiment, configuration, channel setup,
                    calibration, firmware version (rewritten on every change)
    pulses.csv      one row per delivered pulse (current monitor statistics)
    events.csv      markers, thresholds, runs, warnings
    commands.log    every line sent to / received from the board
    waves/wave_NNNN.csv   requested waveforms
"""
from __future__ import annotations

import csv
import datetime as dt
import json
import re
import time
from pathlib import Path
from typing import Optional

from .fwproto import PulseReport, Waveform
from .model import Session, load_json, save_json
from .units import counts_to_ma, permille_to_ma

SESSION_FILE = "session.json"
PHASE_FIELDS = ("pol", "n", "max", "mean", "median", "min", "max_ma", "mean_ma", "median_ma", "min_ma")
PULSE_COLUMNS = (["t_host_s", "run", "t_run_s", "frame", "slot", "channel", "label", "amp_permille", "amp_ma",
                  "baseline"] + [f"p{p}_{f}" for p in (1, 2) for f in PHASE_FIELDS])
EVENT_COLUMNS = ["t_host_s", "wall_time", "kind", "text"]
FLUSH_INTERVAL_S = 1.0


def _slug(text: str) -> str:
    text = re.sub(r"[^A-Za-z0-9_-]+", "-", text.strip())
    return text.strip("-")[:40] or "x"


def _now_iso() -> str:
    return dt.datetime.now().astimezone().isoformat(timespec="seconds")


def _fmt(value) -> str:
    if value is None:
        return ""
    if isinstance(value, float):
        return f"{value:.4f}"
    return str(value)


class SessionRecorder:
    def __init__(self, root: Path, session: Session) -> None:
        stamp = dt.datetime.now().strftime("%Y-%m-%d_%H%M%S")
        name = f"{stamp}_{_slug(session.subject.code)}_{_slug(session.experiment.protocol_name or 'sesion')}"
        folder = Path(root) / name
        suffix = 1
        while folder.exists():
            suffix += 1
            folder = Path(root) / f"{name}_{suffix}"
        (folder / "waves").mkdir(parents=True)
        self.folder = folder
        self.session = session
        if not session.created:
            session.created = _now_iso()
        self.t0 = time.monotonic()
        self.run = 0
        self.run_period_us = 0
        self.run_slots = 1
        self.wave_count = 0
        self._last_flush = self.t0

        self._pulses_file = open(folder / "pulses.csv", "w", newline="", encoding="utf-8")
        self._pulses = csv.writer(self._pulses_file)
        self._pulses.writerow(PULSE_COLUMNS)
        self._events_file = open(folder / "events.csv", "w", newline="", encoding="utf-8")
        self._events = csv.writer(self._events_file)
        self._events.writerow(EVENT_COLUMNS)
        self._log_file = open(folder / "commands.log", "w", encoding="utf-8")
        self.save()
        self.event("session", f"Sesión iniciada en {folder.name}")

    # ------------------------------------------------------------- records
    def elapsed(self, host_time: Optional[float] = None) -> float:
        return (time.monotonic() if host_time is None else host_time) - self.t0

    def save(self) -> None:
        save_json(self.folder / SESSION_FILE, "session", self.session)

    def start_run(self) -> int:
        """Called before every RUN: pulse times restart at each run."""
        timing = self.session.config.timing
        self.run += 1
        self.run_period_us = timing.period_us
        self.run_slots = max(1, len(timing.sequence))
        return self.run

    def pulse(self, report: PulseReport, host_time: float) -> None:
        config = self.session.config
        channel = config.channels[report.channel] if 0 <= report.channel < len(config.channels) else None
        t_run = (report.frame * self.run_period_us + report.slot * self.run_period_us // self.run_slots) / 1e6
        row = [f"{self.elapsed(host_time):.4f}", self.run, f"{t_run:.6f}", report.frame, report.slot,
               report.channel, channel.label if channel else "", report.amplitude,
               _fmt(permille_to_ma(report.amplitude, config.ds8r)), report.baseline]
        cal = config.calibration.ma_per_count
        for p in range(2):
            if p < len(report.phases):
                ph = report.phases[p]
                row += ["+" if ph.positive else "-", ph.n, ph.max, ph.mean, ph.median, ph.min]
                row += [_fmt(counts_to_ma(v, report.baseline, cal)) for v in (ph.max, ph.mean, ph.median, ph.min)]
            else:
                row += [""] * len(PHASE_FIELDS)
        self._pulses.writerow(row)
        self._maybe_flush()

    def event(self, kind: str, text: str, host_time: Optional[float] = None) -> None:
        self._events.writerow([f"{self.elapsed(host_time):.4f}", _now_iso(), kind, text])
        self._events_file.flush()

    def command(self, direction: str, line: str) -> None:
        self._log_file.write(f"{self.elapsed():.4f}\t{direction}\t{line}\n")
        self._maybe_flush()

    def wave(self, wave: Waveform, host_time: float) -> Path:
        self.wave_count += 1
        path = self.folder / "waves" / f"wave_{self.wave_count:04d}.csv"
        cal = self.session.config.calibration.ma_per_count
        baseline = sorted(wave.samples[:max(1, wave.i_trig)])[max(1, wave.i_trig) // 2] if wave.samples else 0
        with open(path, "w", newline="", encoding="utf-8") as f:
            f.write(f"# t_host_s={self.elapsed(host_time):.4f} run={self.run} frame={wave.frame} slot={wave.slot} "
                    f"channel={wave.channel} amp_permille={wave.amplitude} first_phase={'+' if wave.positive else '-'} "
                    f"biphasic={int(wave.biphasic)} ts_us={wave.ts_us} i_trig={wave.i_trig} "
                    f"i_phase1={wave.i_phase1} i_flip={wave.i_flip} i_end={wave.i_end} baseline={baseline}\n")
            writer = csv.writer(f)
            writer.writerow(["index", "t_us", "counts", "ma"])
            for i, v in enumerate(wave.samples):
                writer.writerow([i, i * wave.ts_us, v, _fmt(counts_to_ma(v, baseline, cal))])
        self.event("wave", f"Forma de onda canal {wave.channel} -> {path.name}", host_time)
        return path

    def _maybe_flush(self) -> None:
        now = time.monotonic()
        if now - self._last_flush >= FLUSH_INTERVAL_S:
            self.flush()

    def flush(self) -> None:
        self._last_flush = time.monotonic()
        for f in (self._pulses_file, self._events_file, self._log_file):
            f.flush()

    def close(self) -> None:
        self.event("session", "Sesión cerrada")
        self.session.closed = _now_iso()
        self.save()
        for f in (self._pulses_file, self._events_file, self._log_file):
            f.close()


# ------------------------------------------------------------ loading ------

def load_session(path: Path) -> Session:
    path = Path(path)
    if path.is_dir():
        path = path / SESSION_FILE
    return load_json(path, Session, "session")


def read_events(folder: Path) -> list[dict]:
    with open(Path(folder) / "events.csv", newline="", encoding="utf-8") as f:
        return list(csv.DictReader(f))


# ------------------------------------------------------------- HDF5 ------

def export_hdf5(folder: Path, target: Optional[Path] = None) -> Path:
    """Copies a session folder into a single HDF5 file:
    attrs session_json; datasets pulses (structured), events (strings);
    group waves/<name> with attrs from the header line."""
    import h5py
    import numpy as np

    folder = Path(folder)
    target = Path(target) if target else folder / "session.h5"
    session_text = (folder / SESSION_FILE).read_text(encoding="utf-8")
    with h5py.File(target, "w") as h5:
        h5.attrs["session_json"] = session_text
        h5.attrs["format"] = "stimmux-session-1"

        with open(folder / "pulses.csv", newline="", encoding="utf-8") as f:
            rows = list(csv.DictReader(f))
        text_columns = {"label", "p1_pol", "p2_pol"}
        dtype = [(c, h5py.string_dtype() if c in text_columns else "f8") for c in PULSE_COLUMNS]
        data = np.zeros(len(rows), dtype=dtype)
        for i, row in enumerate(rows):
            for c in PULSE_COLUMNS:
                value = row.get(c, "")
                if c in text_columns:
                    data[c][i] = value
                else:
                    data[c][i] = float(value) if value != "" else np.nan
        h5.create_dataset("pulses", data=data, compression="gzip")

        events = read_events(folder)
        h5.create_dataset("events", data=np.array([[e[c] for c in EVENT_COLUMNS] for e in events],
                                                  dtype=h5py.string_dtype()).reshape(-1, len(EVENT_COLUMNS)))
        h5["events"].attrs["columns"] = json.dumps(EVENT_COLUMNS)

        waves = h5.create_group("waves")
        for path in sorted((folder / "waves").glob("wave_*.csv")):
            lines = path.read_text(encoding="utf-8").splitlines()
            header = dict(item.split("=", 1) for item in lines[0].lstrip("# ").split())
            values = np.array([[float(x) if x else np.nan for x in line.split(",")] for line in lines[2:]])
            ds = waves.create_dataset(path.stem, data=values.reshape(-1, 4))
            ds.attrs["columns"] = json.dumps(["index", "t_us", "counts", "ma"])
            for key, value in header.items():
                ds.attrs[key] = value
    return target
