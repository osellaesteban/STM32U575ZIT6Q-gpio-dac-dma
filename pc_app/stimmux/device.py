"""Board connection for the Qt application.

Lines arrive on a transport thread and are dispatched in the Qt thread by a
timer, so every signal and callback runs in the GUI thread. Responses
("OK..."/"ERR...") arrive in the order of the commands; at most
MAX_IN_FLIGHT commands are sent without response (the firmware receive
buffer is 512 bytes).
"""
from __future__ import annotations

import queue
import time
from collections import deque
from typing import Callable, Optional

from PySide6.QtCore import QObject, QTimer, Signal

from . import fwproto
from .transport import Transport, create_transport

MAX_IN_FLIGHT = 4
RESPONSE_TIMEOUT_S = 2.0

Callback = Optional[Callable[[str], None]]


class Device(QObject):
    connectedChanged = Signal(bool)
    lineLogged = Signal(str, str)          # direction ("TX"/"RX"), line
    pulseReceived = Signal(object, float)  # PulseReport, host time (monotonic)
    waveReceived = Signal(object, float)   # Waveform, host time
    eventReceived = Signal(str)            # "EVT ..." lines
    statusReceived = Signal(dict)          # parsed STATUS response
    errorOccurred = Signal(str)

    def __init__(self, parent=None) -> None:
        super().__init__(parent)
        self.transport: Optional[Transport] = None
        self.info: dict[str, str] = {}
        self._rx: queue.Queue = queue.Queue()
        self._outbox: deque[tuple[str, Callback]] = deque()
        self._in_flight: deque[tuple[str, Callback, float]] = deque()
        self._waves = fwproto.WaveAssembler()
        self._timer = QTimer(self)
        self._timer.setInterval(10)
        self._timer.timeout.connect(self._drain)

    # ---------------------------------------------------------- connection
    @property
    def connected(self) -> bool:
        return self.transport is not None

    def connect_port(self, port: str, on_ready: Callable[[dict], None] | None = None) -> None:
        self.disconnect_port()
        transport = create_transport(port)
        transport.on_line = lambda line: self._rx.put(("line", line))
        transport.on_closed = lambda reason: self._rx.put(("closed", reason))
        transport.open()
        self.transport = transport
        self._timer.start()
        self.connectedChanged.emit(True)

        def info(line: str) -> None:
            self.info = fwproto.parse_kv(line) if line.startswith("OK") else {}
            if on_ready is not None:
                on_ready(self.info)

        # safe known state: stopped, no reports
        self.send("STOP")
        self.send("MON 0")
        self.send("WDOG 0")
        self.send("INFO", info)

    def disconnect_port(self, reason: str = "") -> None:
        if self.transport is None:
            return
        try:
            self.transport.write_line("STOP")
        except Exception:
            pass
        self.transport.close()
        self.transport = None
        self._timer.stop()
        self._outbox.clear()
        self._in_flight.clear()
        self.connectedChanged.emit(False)
        if reason:
            self.errorOccurred.emit(f"Conexión cerrada: {reason}")

    # ------------------------------------------------------------ commands
    def send(self, command: str, callback: Callback = None) -> bool:
        if self.transport is None:
            return False
        self._outbox.append((command, callback))
        self._pump()
        return True

    def send_all(self, commands: list[str], done: Optional[Callable[[list[str]], None]] = None) -> bool:
        """Sends a list of commands; done receives the error responses."""
        if not commands:
            if done:
                done([])
            return self.connected
        errors: list[str] = []
        remaining = [len(commands)]

        def collect(command: str):
            def callback(line: str) -> None:
                if not line.startswith("OK"):
                    errors.append(f"{command} -> {line}")
                remaining[0] -= 1
                if remaining[0] == 0 and done:
                    done(errors)
            return callback

        ok = True
        for command in commands:
            ok = self.send(command, collect(command)) and ok
        return ok

    def emergency_stop(self) -> None:
        """STOP ahead of every queued command."""
        if self.transport is None:
            return
        self._outbox.clear()
        self.transport.write_line("STOP")
        self.lineLogged.emit("TX", "STOP")
        self._in_flight.append(("STOP", None, time.monotonic()))

    def _pump(self) -> None:
        while self.transport is not None and self._outbox and len(self._in_flight) < MAX_IN_FLIGHT:
            command, callback = self._outbox.popleft()
            self._in_flight.append((command, callback, time.monotonic()))
            self.transport.write_line(command)
            self.lineLogged.emit("TX", command)

    # ----------------------------------------------------------- reception
    def _drain(self) -> None:
        now = time.monotonic()
        while True:
            try:
                kind, line = self._rx.get_nowait()
            except queue.Empty:
                break
            if kind == "closed":
                self.disconnect_port(line)
                return
            self._dispatch(line, now)
        if self._in_flight and now - self._in_flight[0][2] > RESPONSE_TIMEOUT_S:
            command, callback, _ = self._in_flight.popleft()
            self.errorOccurred.emit(f"Sin respuesta a '{command}'")
            if callback:
                callback("ERR TIMEOUT")
        self._pump()

    def _dispatch(self, line: str, now: float) -> None:
        if fwproto.is_wave_line(line):
            wave = self._waves.feed(line)
            if wave is not None:
                self.lineLogged.emit("RX", f"WAVE canal {wave.channel}, {wave.n} muestras")
                self.waveReceived.emit(wave, now)
            return
        if line.startswith("P "):
            report = fwproto.parse_pulse(line)
            if report is not None:
                self.pulseReceived.emit(report, now)
            return
        self.lineLogged.emit("RX", line)
        if fwproto.is_response(line):
            if not self._in_flight:
                return
            command, callback, _ = self._in_flight.popleft()
            if command == "STATUS" and line.startswith("OK"):
                self.statusReceived.emit(fwproto.parse_kv(line))
            if callback:
                callback(line)
        elif line.startswith("EVT"):
            self.eventReceived.emit(line)
