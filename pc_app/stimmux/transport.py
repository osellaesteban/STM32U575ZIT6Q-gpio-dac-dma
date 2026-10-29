"""Line transports: the real serial port and the in-process simulator."""
from __future__ import annotations

import queue
import threading
import time
from typing import Callable, Optional

from .simulator import SimulatedBoard

SIMULATOR_PORT = "Simulador"
DEFAULT_BAUDRATE = 921600


def list_ports() -> list[tuple[str, str]]:
    """[(device, description)], the simulator first."""
    ports = [(SIMULATOR_PORT, "Placa simulada (sin hardware)")]
    try:
        from serial.tools import list_ports as lp
        for p in sorted(lp.comports(), key=lambda p: p.device):
            ports.append((p.device, p.description or p.device))
    except ImportError:
        pass
    return ports


class Transport:
    """Delivers each received line to on_line from a background thread."""

    def __init__(self) -> None:
        self.on_line: Optional[Callable[[str], None]] = None
        self.on_closed: Optional[Callable[[str], None]] = None
        self._thread: Optional[threading.Thread] = None
        self._stop = threading.Event()

    def open(self) -> None:
        self._stop.clear()
        self._thread = threading.Thread(target=self._run, daemon=True)
        self._thread.start()

    def close(self) -> None:
        self._stop.set()
        if self._thread is not None and self._thread is not threading.current_thread():
            self._thread.join(timeout=1.0)
        self._thread = None

    def write_line(self, line: str) -> None:
        raise NotImplementedError

    def _run(self) -> None:
        raise NotImplementedError

    def _deliver(self, line: str) -> None:
        if self.on_line is not None:
            self.on_line(line)


class SerialTransport(Transport):
    def __init__(self, port: str, baudrate: int = DEFAULT_BAUDRATE) -> None:
        super().__init__()
        import serial
        self._serial = serial.Serial()
        self._serial.port = port
        self._serial.baudrate = baudrate
        self._serial.timeout = 0.05
        self._write_lock = threading.Lock()

    def open(self) -> None:
        self._serial.open()
        self._serial.reset_input_buffer()
        super().open()

    def close(self) -> None:
        super().close()
        if self._serial.is_open:
            self._serial.close()

    def write_line(self, line: str) -> None:
        with self._write_lock:
            self._serial.write((line + "\n").encode("ascii", "replace"))

    def _run(self) -> None:
        buffer = b""
        try:
            while not self._stop.is_set():
                data = self._serial.read(self._serial.in_waiting or 1)
                if not data:
                    continue
                buffer += data
                while True:
                    cut = min((i for i in (buffer.find(b"\n"), buffer.find(b"\r")) if i >= 0), default=-1)
                    if cut < 0:
                        break
                    line, buffer = buffer[:cut], buffer[cut + 1:]
                    if line:
                        self._deliver(line.decode("ascii", "replace").strip())
        except Exception as exc:  # port unplugged, etc.
            if not self._stop.is_set() and self.on_closed is not None:
                self.on_closed(str(exc))


class SimulatedTransport(Transport):
    """Runs a SimulatedBoard; responses keep the order of the commands."""

    def __init__(self, board: Optional[SimulatedBoard] = None, tick_s: float = 0.005) -> None:
        super().__init__()
        self.board = board or SimulatedBoard()
        self.tick_s = tick_s
        self._inbox: queue.Queue[str] = queue.Queue()

    def write_line(self, line: str) -> None:
        self._inbox.put(line)

    def _run(self) -> None:
        while not self._stop.is_set():
            try:
                line = self._inbox.get(timeout=self.tick_s)
                self._deliver(self.board.handle(line))
            except queue.Empty:
                pass
            for out in self.board.tick(time.monotonic()):
                self._deliver(out)


def create_transport(port: str, baudrate: int = DEFAULT_BAUDRATE) -> Transport:
    if port == SIMULATOR_PORT:
        return SimulatedTransport()
    return SerialTransport(port, baudrate)
