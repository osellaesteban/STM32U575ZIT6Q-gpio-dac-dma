"""Data model: subject, experiment, DS8R settings, channels, timing, protocol
and session, with JSON (de)serialization.

Files:
    subjects/<code>.json      Subject
    *.stimcfg.json             StimConfig (reusable template: timing, DS8R,
                               channel definitions and protocol)
    <session folder>/session.json   Session (everything, see storage.py)

Amplitudes are always stored in mA. The firmware works in per mil of the
DS8R full scale, see units.py.
"""
from __future__ import annotations

import dataclasses
import json
from dataclasses import dataclass, field
from pathlib import Path
from typing import Any, Optional

SCHEMA_VERSION = 1
CHANNELS = 8
MAX_SLOTS = 32
LABEL_MAX = 15

POLARITIES = ("POS", "NEG", "ALT", "BIPH")
POLARITY_NAMES = {
    "POS": "Positiva",
    "NEG": "Negativa",
    "ALT": "Alternada",
    "BIPH": "Bifásica (experimental)",
}
BLOCK_KINDS = ("ramp", "hold", "rest", "repeat")
BLOCK_NAMES = {"ramp": "Rampa", "hold": "Meseta", "rest": "Descanso", "repeat": "Repetir"}
UNITS = ("percent", "ma")
UNIT_NAMES = {"percent": "% nivel de trabajo", "ma": "mA"}


@dataclass
class Subject:
    code: str = ""
    birth_year: Optional[int] = None
    sex: str = ""                  # F / M / X / ""
    handedness: str = ""           # Diestro / Zurdo / Ambidiestro / ""
    weight_kg: Optional[float] = None
    height_cm: Optional[float] = None
    condition: str = ""            # clinical condition / diagnosis
    notes: str = ""


@dataclass
class Experiment:
    study: str = ""
    protocol_name: str = ""
    operator: str = ""
    ethics_code: str = ""
    consent_obtained: bool = False
    consent_notes: str = ""
    notes: str = ""


@dataclass
class DS8RSettings:
    """Settings entered by hand on the DS8R (the board can't read them)."""
    range_ma: float = 100.0         # current at 10 V on the control input
    pulse_width_us: int = 200       # total width; must match the firmware PW
    mode: str = "Monofásico positivo"
    current_limit_ma: float = 100.0
    notes: str = ""


@dataclass
class ChannelSetup:
    index: int = 0
    enabled: bool = False
    label: str = ""                 # sent to the firmware (no spaces, <= 15 chars)
    target: str = ""                # muscle / nerve
    side: str = ""                  # Derecho / Izquierdo / ""
    polarity: str = "POS"
    electrode_type: str = ""
    electrode_size: str = ""
    electrode_location: str = ""
    impedance_kohm: Optional[float] = None
    threshold_sensory_ma: Optional[float] = None
    threshold_motor_ma: Optional[float] = None
    threshold_max_ma: Optional[float] = None
    working_level_ma: float = 0.0   # reference for "percent" protocol blocks
    safety_limit_ma: float = 0.0    # never exceeded; 0 = channel can't stimulate

    def firmware_label(self) -> str:
        text = self.label or f"CH{self.index}"
        text = "".join(c if c.isalnum() or c in "_-" else "_" for c in text)
        return text[:LABEL_MAX]


@dataclass
class Timing:
    period_us: int = 25000
    sequence: list[int] = field(default_factory=lambda: list(range(CHANNELS)))
    settle_us: int = 50
    trigger_us: int = 10
    ds8r_delay_us: int = 10
    tail_us: int = 50
    dac_full_scale: int = 4095
    monitor_trim_us: int = 4

    def channel_rate_hz(self, channel: int) -> float:
        return self.sequence.count(channel) * 1e6 / self.period_us


@dataclass
class Calibration:
    ma_per_count: float = 0.0       # measured current scale, 0 = not calibrated


@dataclass
class ProtocolBlock:
    kind: str = "hold"
    duration_s: float = 1.0
    unit: str = "percent"
    start: list[float] = field(default_factory=lambda: [0.0] * CHANNELS)
    end: list[float] = field(default_factory=lambda: [0.0] * CHANNELS)
    repeat_span: int = 1            # "repeat": number of previous blocks
    repeat_count: int = 1           # "repeat": extra repetitions


@dataclass
class Protocol:
    name: str = ""
    blocks: list[ProtocolBlock] = field(default_factory=list)


@dataclass
class StimConfig:
    """Reusable configuration template."""
    timing: Timing = field(default_factory=Timing)
    ds8r: DS8RSettings = field(default_factory=DS8RSettings)
    channels: list[ChannelSetup] = field(default_factory=lambda: default_channels())
    protocol: Protocol = field(default_factory=Protocol)
    calibration: Calibration = field(default_factory=Calibration)


@dataclass
class Session:
    subject: Subject = field(default_factory=Subject)
    experiment: Experiment = field(default_factory=Experiment)
    config: StimConfig = field(default_factory=StimConfig)
    created: str = ""               # ISO 8601, local time
    closed: str = ""
    app_version: str = ""
    firmware: str = ""
    notes: str = ""


def default_channels() -> list[ChannelSetup]:
    return [ChannelSetup(index=i, label=f"CH{i}") for i in range(CHANNELS)]


# ---------------------------------------------------------------- JSON ------

def to_dict(obj: Any) -> dict:
    return dataclasses.asdict(obj)


def _build(cls, data: Any):
    """Builds a dataclass from a dict, ignoring unknown keys and keeping
    defaults for missing ones (files written by other versions)."""
    if not dataclasses.is_dataclass(cls) or not isinstance(data, dict):
        return data
    kwargs = {}
    hints = {f.name: f for f in dataclasses.fields(cls)}
    for name, f in hints.items():
        if name not in data:
            continue
        value = data[name]
        ftype = _NESTED.get((cls, name))
        if ftype is not None:
            if isinstance(value, list):
                value = [_build(ftype, v) for v in value]
            else:
                value = _build(ftype, value)
        kwargs[name] = value
    return cls(**kwargs)


_NESTED = {
    (StimConfig, "timing"): Timing,
    (StimConfig, "ds8r"): DS8RSettings,
    (StimConfig, "channels"): ChannelSetup,
    (StimConfig, "protocol"): Protocol,
    (StimConfig, "calibration"): Calibration,
    (Protocol, "blocks"): ProtocolBlock,
    (Session, "subject"): Subject,
    (Session, "experiment"): Experiment,
    (Session, "config"): StimConfig,
}


def from_dict(cls, data: dict):
    obj = _build(cls, data)
    if isinstance(obj, StimConfig):
        _normalize_config(obj)
    elif isinstance(obj, Session):
        _normalize_config(obj.config)
    return obj


def _normalize_config(config: StimConfig) -> None:
    channels = {c.index: c for c in config.channels if 0 <= c.index < CHANNELS}
    config.channels = [channels.get(i, ChannelSetup(index=i, label=f"CH{i}")) for i in range(CHANNELS)]
    for block in config.protocol.blocks:
        block.start = (list(block.start) + [0.0] * CHANNELS)[:CHANNELS]
        block.end = (list(block.end) + [0.0] * CHANNELS)[:CHANNELS]


def save_json(path: Path, kind: str, obj: Any) -> None:
    data = {"schema": SCHEMA_VERSION, "kind": kind, "data": to_dict(obj)}
    path = Path(path)
    tmp = path.with_suffix(path.suffix + ".tmp")
    tmp.write_text(json.dumps(data, indent=2, ensure_ascii=False), encoding="utf-8")
    tmp.replace(path)


def load_json(path: Path, cls, kind: str):
    data = json.loads(Path(path).read_text(encoding="utf-8"))
    if data.get("kind") != kind:
        raise ValueError(f"{path} no es un archivo de tipo '{kind}' (es '{data.get('kind')}')")
    if data.get("schema", 0) > SCHEMA_VERSION:
        raise ValueError(f"{path} fue creado con una versión más nueva de la aplicación")
    return from_dict(cls, data["data"])
