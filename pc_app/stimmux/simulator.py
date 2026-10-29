"""In-process simulation of the board firmware (same line protocol).

It is used by the tests and to try the application without hardware
("Simulador" in the port list). The current monitor returns a synthetic
current proportional to the amplitude, with noise.
"""
from __future__ import annotations

import random
import time
from typing import Optional

from . import FIRMWARE_VERSION
from .model import CHANNELS, DS8RSettings, Timing
from .units import AMP_MAX, check_timing

POLARITIES = ("POS", "NEG", "ALT", "BIPH")
SIM_BASELINE = 100              # ADC counts with zero current
SIM_COUNTS_PER_PERMILLE = 3.5   # synthetic current scale
SIM_NOISE = 6
MON_TS_US = 2


class SimulatedBoard:
    def __init__(self, seed: Optional[int] = None) -> None:
        self.rng = random.Random(seed)
        self.reset()

    def reset(self) -> None:
        self.amp = [0] * CHANNELS
        self.en = [False] * CHANNELS
        self.pol = ["POS"] * CHANNELS
        self.label = [f"Channel {i}" for i in range(CHANNELS)]
        self.timing = Timing()
        self.pw = 200
        self.running = False
        self.frames = 0
        self.target = 0
        self.wdog_ms = 0
        self.mon = False
        self.trim = 4
        self.wave_channel: Optional[int] = None    # -1 any channel
        self.alt = [False] * CHANNELS
        self.start_time = 0.0
        self.next_slot = 0                          # absolute slot counter
        self.last_command = time.monotonic()
        self.pending: list[str] = ["EVT READY STIMMUX FW=" + FIRMWARE_VERSION]

    # ------------------------------------------------------------ commands
    def handle(self, line: str, now: Optional[float] = None) -> str:
        now = time.monotonic() if now is None else now
        self.last_command = now
        tokens = line.replace(",", " ").split()
        if not tokens:
            return "ERR ARG"
        cmd = tokens[0].upper()
        args = tokens[1:]
        try:
            return self._dispatch(cmd, args, now)
        except (ValueError, IndexError):
            return "ERR ARG"

    def _uint(self, text: str, maximum: int) -> int:
        if not text.isdigit():
            raise ValueError(text)
        value = int(text)
        if value > maximum:
            raise ValueError(text)
        return value

    def _dispatch(self, cmd: str, args: list[str], now: float) -> str:
        if cmd == "PING":
            return "OK PONG"
        if cmd == "INFO":
            return f"OK STIMMUX FW={FIRMWARE_VERSION} CH={CHANNELS} SLOTS=32 AMPMAX={AMP_MAX} PWMIN=50 PWMAX=2000 MININT=1000 SIM=1"
        if cmd == "STATUS":
            return (f"OK RUN={int(self.running)} FRAMES={self.frames} WDOG={self.wdog_ms} MON={int(self.mon)} "
                    f"TRIM={self.trim} MLOST=0 TXDROP=0 MSYNC=0")
        if cmd == "GET":
            t = self.timing
            return (f"OK PERIOD={t.period_us} SETTLE={t.settle_us} TRIG={t.trigger_us} DELAY={t.ds8r_delay_us} "
                    f"PW={self.pw} TAIL={t.tail_us} DACFS={t.dac_full_scale} SEQ=" + ",".join(map(str, t.sequence)))
        if cmd == "STOP":
            self.running = False
            return "OK"
        if cmd == "RUN":
            if self.running:
                return "ERR BUSY"
            frames = self._uint(args[0], 0xFFFFFFFF) if args else 0
            ds8r = DS8RSettings(pulse_width_us=self.pw)
            if check_timing(self.timing, ds8r):
                return "ERR TIMING"
            self.running = True
            self.frames = 0
            self.target = frames
            self.alt = [False] * CHANNELS
            self.start_time = now + 0.002
            self.next_slot = 0
            return "OK"
        if cmd == "WDOG":
            self.wdog_ms = self._uint(args[0], 600000)
            return "OK"
        if cmd == "MON":
            self.mon = bool(self._uint(args[0], 1))
            return "OK"
        if cmd == "MONTRIM":
            value = self._uint(args[0], 0xFFFF)
            if value > 500:
                return "ERR RANGE"
            self.trim = value
            return "OK"
        if cmd == "WAVE":
            ch = self._uint(args[0], 255) if args else -1
            if ch >= CHANNELS:
                return "ERR CHANNEL"
            if self.wave_channel is not None:
                return "ERR BUSY"
            self.wave_channel = ch
            return "OK"
        if cmd == "CH":
            return self._channel(args)
        if cmd in ("AMPS", "ENS"):
            values = [self._uint(a, 0xFFFF) for a in args]
            if not 1 <= len(values) <= CHANNELS:
                return "ERR ARG"
            limit = AMP_MAX if cmd == "AMPS" else 1
            if any(v > limit for v in values):
                return "ERR RANGE"
            for i, v in enumerate(values):
                if cmd == "AMPS":
                    self.amp[i] = v
                else:
                    self.en[i] = bool(v)
            return "OK"
        if cmd in ("PERIOD", "SEQ", "TIMING", "PW", "DACFS", "DEFAULT"):
            if self.running:
                return "ERR BUSY"
            return self._config(cmd, args)
        return "ERR UNKNOWN"

    def _channel(self, args: list[str]) -> str:
        ch = self._uint(args[0], 255)
        if ch >= CHANNELS:
            return "ERR CHANNEL"
        if len(args) == 1:
            return (f"OK CH={ch} EN={int(self.en[ch])} AMP={self.amp[ch]} POL={self.pol[ch]} "
                    f"LABEL={self.label[ch]}")
        if len(args) != 3:
            return "ERR ARG"
        key, value = args[1].upper(), args[2]
        if key == "EN":
            self.en[ch] = bool(self._uint(value, 1))
        elif key == "AMP":
            amp = self._uint(value, 0xFFFF)
            if amp > AMP_MAX:
                return "ERR RANGE"
            self.amp[ch] = amp
        elif key == "POL":
            if value.upper() not in POLARITIES:
                return "ERR ARG"
            self.pol[ch] = value.upper()
        elif key == "LABEL":
            if len(value) > 15:
                return "ERR RANGE"
            self.label[ch] = value
        else:
            return "ERR ARG"
        return "OK"

    def _config(self, cmd: str, args: list[str]) -> str:
        t = self.timing
        if cmd == "PERIOD":
            value = self._uint(args[0], 0xFFFFFFFF)
            if not 0 < value <= 10_000_000:
                return "ERR RANGE"
            t.period_us = value
        elif cmd == "SEQ":
            seq = [self._uint(a, 255) for a in args]
            if not 1 <= len(seq) <= 32:
                return "ERR ARG"
            if any(c >= CHANNELS for c in seq):
                return "ERR CHANNEL"
            t.sequence = seq
        elif cmd == "TIMING":
            if len(args) != 4:
                return "ERR ARG"
            values = [self._uint(a, 0xFFFF) for a in args]
            if min(values[0], values[1], values[3]) < 5:
                return "ERR RANGE"
            t.settle_us, t.trigger_us, t.ds8r_delay_us, t.tail_us = values
        elif cmd == "PW":
            value = self._uint(args[0], 0xFFFF)
            if not 50 <= value <= 2000:
                return "ERR RANGE"
            self.pw = value
        elif cmd == "DACFS":
            value = self._uint(args[0], 0xFFFF)
            if not 0 < value <= 4095:
                return "ERR RANGE"
            t.dac_full_scale = value
        elif cmd == "DEFAULT":
            self.reset()
            self.pending.clear()
        return "OK"

    # --------------------------------------------------------------- time
    def tick(self, now: Optional[float] = None) -> list[str]:
        """Advances the simulation and returns the asynchronous lines."""
        now = time.monotonic() if now is None else now
        out, self.pending = self.pending, []
        if not self.running:
            return out
        if self.wdog_ms and (now - self.last_command) * 1000 > self.wdog_ms:
            self.running = False
            out.append("EVT WDOG")
            return out
        t = self.timing
        n = len(t.sequence)
        slot_s = t.period_us / n / 1e6
        produced = 0
        while self.running and self.start_time + (self.next_slot + 1) * slot_s <= now and produced < 500:
            frame, slot = divmod(self.next_slot, n)
            self._pulse(frame, slot, out)
            self.next_slot += 1
            produced += 1
            if slot == n - 1:
                self.frames = frame + 1
                if self.target and self.frames >= self.target:
                    self.running = False
                    out.append(f"EVT DONE FRAMES={self.frames}")
        return out

    def _pulse(self, frame: int, slot: int, out: list[str]) -> None:
        t = self.timing
        ch = t.sequence[slot]
        amp = self.amp[ch]
        if not self.en[ch] or amp == 0:
            return
        pol = self.pol[ch]
        if pol == "NEG":
            positive = False
        elif pol == "ALT":
            positive = not self.alt[ch]
            self.alt[ch] = positive
        else:
            positive = True
        biphasic = pol == "BIPH"
        level = SIM_BASELINE + amp * SIM_COUNTS_PER_PERMILLE
        if self.mon:
            phases = 2 if biphasic else 1
            n_phase = max(1, (self.pw // phases - 2 * self.trim) // MON_TS_US)
            line = f"P {frame} {slot} {ch} {amp} {SIM_BASELINE}"
            sign = positive
            for _ in range(phases):
                values = sorted(int(level + self.rng.gauss(0, SIM_NOISE)) for _ in range(n_phase))
                mean = int(round(sum(values) / len(values)))
                line += f" {'+' if sign else '-'} {n_phase} {values[-1]} {mean} {values[(n_phase - 1) // 2]} {values[0]}"
                sign = not sign
            out.append(line)
        if self.wave_channel is not None and self.wave_channel in (-1, ch):
            self.wave_channel = None
            out.extend(self._wave(frame, slot, ch, amp, positive, biphasic, level))

    def _wave(self, frame, slot, ch, amp, positive, biphasic, level) -> list[str]:
        t = self.timing
        i_trig = t.settle_us // MON_TS_US
        i_phase1 = (t.settle_us + t.ds8r_delay_us) // MON_TS_US
        i_flip = (t.settle_us + t.ds8r_delay_us + self.pw // 2) // MON_TS_US if biphasic else 0
        i_end = (t.settle_us + t.ds8r_delay_us + self.pw) // MON_TS_US
        n = (t.settle_us + t.ds8r_delay_us + self.pw + t.tail_us) // MON_TS_US
        samples = []
        for i in range(n):
            value = level if i_phase1 <= i < i_end else SIM_BASELINE
            if biphasic and i in (i_flip, i_flip - 1):
                value = SIM_BASELINE + (level - SIM_BASELINE) * 0.3
            samples.append(max(0, min(4095, int(value + self.rng.gauss(0, SIM_NOISE)))))
        lines = [f"WH {frame} {slot} {ch} {amp} {'+' if positive else '-'} {int(biphasic)} {MON_TS_US} {n} "
                 f"{i_trig} {i_phase1} {i_flip} {i_end}"]
        for start in range(0, n, 24):
            lines.append(f"WD {start} " + " ".join(str(v) for v in samples[start:start + 24]))
        lines.append("WE")
        return lines
