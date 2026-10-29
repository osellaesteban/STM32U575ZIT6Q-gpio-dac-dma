"""Firmware line protocol: command builders and parsers of responses and
asynchronous reports (see the firmware README)."""
from __future__ import annotations

from dataclasses import dataclass, field
from typing import Optional

from .model import CHANNELS, StimConfig


def is_response(line: str) -> bool:
    return line == "OK" or line.startswith("OK ") or line.startswith("ERR")


def parse_kv(line: str) -> dict[str, str]:
    """'OK RUN=1 FRAMES=10' -> {'RUN': '1', 'FRAMES': '10'}"""
    result = {}
    for token in line.split()[1:]:
        if "=" in token:
            key, value = token.split("=", 1)
            result[key] = value
    return result


# ----------------------------------------------------------- reports ------

@dataclass
class PhaseStats:
    positive: bool
    n: int
    max: int
    mean: int
    median: int
    min: int


@dataclass
class PulseReport:
    frame: int
    slot: int
    channel: int
    amplitude: int              # per mil
    baseline: int
    phases: list[PhaseStats] = field(default_factory=list)


def parse_pulse(line: str) -> Optional[PulseReport]:
    """P <frame> <slot> <ch> <amp> <base> (<+|-> <n> <max> <mean> <med> <min>){1,2}"""
    parts = line.split()
    if not parts or parts[0] != "P" or len(parts) not in (12, 18):
        return None
    try:
        report = PulseReport(*(int(v) for v in parts[1:6]))
        for start in range(6, len(parts), 6):
            sign = parts[start]
            if sign not in "+-":
                return None
            values = [int(v) for v in parts[start + 1:start + 6]]
            report.phases.append(PhaseStats(sign == "+", *values))
    except ValueError:
        return None
    return report


@dataclass
class Waveform:
    frame: int
    slot: int
    channel: int
    amplitude: int
    positive: bool
    biphasic: bool
    ts_us: int
    n: int
    i_trig: int
    i_phase1: int
    i_flip: int
    i_end: int
    samples: list[int] = field(default_factory=list)


class WaveAssembler:
    """Collects WH / WD / WE lines into a Waveform."""

    def __init__(self) -> None:
        self.current: Optional[Waveform] = None

    def feed(self, line: str) -> Optional[Waveform]:
        parts = line.split()
        if not parts:
            return None
        try:
            if parts[0] == "WH" and len(parts) == 13:
                v = parts[1:]
                self.current = Waveform(int(v[0]), int(v[1]), int(v[2]), int(v[3]), v[4] == "+",
                                        v[5] == "1", int(v[6]), int(v[7]), int(v[8]), int(v[9]),
                                        int(v[10]), int(v[11]))
            elif parts[0] == "WD" and self.current is not None:
                index = int(parts[1])
                if index != len(self.current.samples):
                    self.current = None          # lost lines: discard
                else:
                    self.current.samples.extend(int(v) for v in parts[2:])
            elif parts[0] == "WE" and self.current is not None:
                wave, self.current = self.current, None
                if len(wave.samples) == wave.n:
                    return wave
        except ValueError:
            self.current = None
        return None


def is_wave_line(line: str) -> bool:
    return line[:3] in ("WH ", "WD ") or line == "WE"


# ---------------------------------------------------------- commands ------

def config_commands(config: StimConfig) -> list[str]:
    """Commands that load a configuration (the stimulation must be stopped)."""
    t = config.timing
    commands = [
        f"PERIOD {t.period_us}",
        "SEQ " + " ".join(str(c) for c in t.sequence),
        f"TIMING {t.settle_us} {t.trigger_us} {t.ds8r_delay_us} {t.tail_us}",
        f"PW {config.ds8r.pulse_width_us}",
        f"DACFS {t.dac_full_scale}",
        f"MONTRIM {t.monitor_trim_us}",
    ]
    for ch in config.channels:
        commands.append(f"CH {ch.index} POL {ch.polarity}")
        commands.append(f"CH {ch.index} LABEL {ch.firmware_label()}")
    commands.append(ens_command([False] * CHANNELS))
    commands.append(amps_command([0] * CHANNELS))
    return commands


def amps_command(permille: list[int]) -> str:
    return "AMPS " + " ".join(str(int(v)) for v in permille)


def ens_command(enabled: list[bool]) -> str:
    return "ENS " + " ".join("1" if e else "0" for e in enabled)
