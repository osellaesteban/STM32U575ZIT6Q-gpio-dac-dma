"""Protocol by blocks: expansion into timed segments, validation against the
channel limits and amplitude computation over time.

Block kinds:
    ramp    linear change from "start" to "end" in duration_s
    hold    constant "start" during duration_s
    rest    every channel at 0 during duration_s
    repeat  repeats the segments of the previous repeat_span blocks
            repeat_count more times (it can repeat other repeat blocks)
Values are per channel, in mA or in % of the channel working level.
"""
from __future__ import annotations

from dataclasses import dataclass

from .model import CHANNELS, ChannelSetup, Protocol, ProtocolBlock, StimConfig
from .units import channel_max_ma

MAX_SEGMENTS = 100_000


@dataclass
class Segment:
    block: int                  # index of the originating block
    kind: str
    duration_s: float
    start_ma: list[float]
    end_ma: list[float]


def block_values_ma(block: ProtocolBlock, values: list[float], channels: list[ChannelSetup]) -> list[float]:
    result = []
    for i in range(CHANNELS):
        value = float(values[i]) if i < len(values) else 0.0
        if block.unit == "percent":
            value = channels[i].working_level_ma * value / 100.0
        result.append(value)
    return result


def expand(protocol: Protocol, channels: list[ChannelSetup]) -> list[Segment]:
    segments: list[Segment] = []
    ranges: list[tuple[int, int]] = []          # segments produced by each block
    for index, block in enumerate(protocol.blocks):
        first = len(segments)
        if block.kind == "repeat":
            span = max(0, min(block.repeat_span, index))
            if span and block.repeat_count > 0:
                start = ranges[index - span][0]
                stop = ranges[index - 1][1]
                chunk = segments[start:stop]
                if len(segments) + len(chunk) * block.repeat_count > MAX_SEGMENTS:
                    raise ValueError("El protocolo tiene demasiadas repeticiones.")
                for _ in range(block.repeat_count):
                    segments.extend(Segment(index, s.kind, s.duration_s, s.start_ma, s.end_ma) for s in chunk)
        else:
            zeros = [0.0] * CHANNELS
            if block.kind == "rest":
                start = end = zeros
            else:
                start = block_values_ma(block, block.start, channels)
                end = block_values_ma(block, block.end, channels) if block.kind == "ramp" else start
            segments.append(Segment(index, block.kind, max(0.0, block.duration_s), start, end))
        ranges.append((first, len(segments)))
    return segments


def validate(protocol: Protocol, config: StimConfig) -> list[str]:
    errors = []
    if not protocol.blocks:
        return ["El protocolo no tiene bloques."]
    for index, block in enumerate(protocol.blocks):
        name = f"Bloque {index + 1}"
        if block.kind not in ("ramp", "hold", "rest", "repeat"):
            errors.append(f"{name}: tipo desconocido '{block.kind}'.")
        if block.kind != "repeat" and block.duration_s <= 0:
            errors.append(f"{name}: la duración debe ser mayor que 0.")
        if block.kind == "repeat" and not 1 <= block.repeat_span <= index:
            errors.append(f"{name}: debe repetir entre 1 y {index} bloques anteriores.")
        if block.kind == "repeat" and block.repeat_count < 1:
            errors.append(f"{name}: la cantidad de repeticiones debe ser al menos 1.")
    if errors:
        return errors
    try:
        segments = expand(protocol, config.channels)
    except ValueError as exc:
        return [str(exc)]
    limits = [channel_max_ma(ch, config.ds8r) for ch in config.channels]
    reported = set()
    for seg in segments:
        for ch in range(CHANNELS):
            peak = max(seg.start_ma[ch], seg.end_ma[ch])
            low = min(seg.start_ma[ch], seg.end_ma[ch])
            key = (seg.block, ch)
            if key in reported:
                continue
            if low < 0:
                errors.append(f"Bloque {seg.block + 1}, canal {ch}: valores negativos.")
                reported.add(key)
            elif peak > 0 and not config.channels[ch].enabled:
                errors.append(f"Bloque {seg.block + 1}, canal {ch}: el canal está deshabilitado.")
                reported.add(key)
            elif peak > limits[ch] + 1e-9:
                errors.append(f"Bloque {seg.block + 1}, canal {ch}: {peak:.2f} mA supera el límite "
                              f"({limits[ch]:.2f} mA).")
                reported.add(key)
    if not errors and total_duration(segments) <= 0:
        errors.append("El protocolo dura 0 s.")
    return errors


def total_duration(segments: list[Segment]) -> float:
    return sum(s.duration_s for s in segments)


class ProtocolRun:
    """Amplitudes over time of an expanded protocol."""

    def __init__(self, segments: list[Segment]) -> None:
        self.segments = segments
        self.total_s = total_duration(segments)
        self._index = 0
        self._segment_start = 0.0

    def finished(self, t: float) -> bool:
        return t >= self.total_s

    def at(self, t: float) -> tuple[list[float], int]:
        """(amplitudes in mA, segment index) at t seconds from the start.
        t must not decrease between calls."""
        if not self.segments or t >= self.total_s:
            return [0.0] * CHANNELS, len(self.segments)
        while self._index < len(self.segments) - 1 and \
                t >= self._segment_start + self.segments[self._index].duration_s:
            self._segment_start += self.segments[self._index].duration_s
            self._index += 1
        seg = self.segments[self._index]
        if seg.kind != "ramp" or seg.duration_s <= 0:
            return list(seg.start_ma), self._index
        f = min(1.0, max(0.0, (t - self._segment_start) / seg.duration_s))
        return [a + (b - a) * f for a, b in zip(seg.start_ma, seg.end_ma)], self._index
