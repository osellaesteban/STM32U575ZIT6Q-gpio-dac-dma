"""Application logic shared by the windows: session lifecycle, board
configuration, stimulation modes and safety rules.

Modes:
    idle       nothing running
    manual     continuous RUN, amplitudes from the manual controls
    protocol   continuous RUN, amplitudes from a ProtocolRun (timer)
    threshold  continuous RUN of a single channel with a step ramp

Safety:
    - every amplitude is clamped to the channel maximum (safety limit,
      DS8R range and current limit); in threshold mode to the ramp cap
    - while running, the firmware watchdog is enabled and kept alive with
      STATUS requests: if the application hangs the board stops by itself
    - stimulation requires an open session, so everything is recorded
"""
from __future__ import annotations

import copy
import time
from pathlib import Path
from typing import Optional

from PySide6.QtCore import QObject, QTimer, Signal

from . import FIRMWARE_VERSION, __version__, fwproto, runner, units
from .device import Device
from .model import CHANNELS, Session
from .storage import SessionRecorder

WATCHDOG_MS = 1000
KEEPALIVE_MS = 250
PROTOCOL_TICK_MS = 50
SAVE_DELAY_MS = 1000


class Controller(QObject):
    stateChanged = Signal(str)
    sessionChanged = Signal()           # session object replaced, opened or closed
    message = Signal(str, str)          # level (info / warning / error), text
    protocolProgress = Signal(float, float, int)   # t, total, block index
    amplitudesSent = Signal(list)       # mA per channel
    statusUpdated = Signal(dict)
    thresholdLevel = Signal(float)      # threshold ramp amplitude (mA)

    def __init__(self, parent=None) -> None:
        super().__init__(parent)
        self.device = Device(self)
        self.session = Session()
        self.recorder: Optional[SessionRecorder] = None
        self.state = "idle"
        self._sent_permille: list[int] = [-1] * CHANNELS
        self._manual_ma = [0.0] * CHANNELS
        self._run: Optional[runner.ProtocolRun] = None
        self._run_start = 0.0
        self._threshold_channel = -1
        self._threshold_level = 0.0
        self._threshold_step = 0.0
        self._threshold_cap = 0.0

        self._keepalive = QTimer(self)
        self._keepalive.setInterval(KEEPALIVE_MS)
        self._keepalive.timeout.connect(lambda: self.device.send("STATUS"))
        self._protocol_timer = QTimer(self)
        self._protocol_timer.setInterval(PROTOCOL_TICK_MS)
        self._protocol_timer.timeout.connect(self._protocol_tick)
        self._threshold_timer = QTimer(self)
        self._threshold_timer.timeout.connect(self._threshold_tick)
        self._save_timer = QTimer(self)
        self._save_timer.setSingleShot(True)
        self._save_timer.setInterval(SAVE_DELAY_MS)
        self._save_timer.timeout.connect(self._save_now)

        self.device.pulseReceived.connect(self._on_pulse)
        self.device.waveReceived.connect(self._on_wave)
        self.device.eventReceived.connect(self._on_event)
        self.device.statusReceived.connect(self.statusUpdated)
        self.device.lineLogged.connect(self._on_line)
        self.device.errorOccurred.connect(lambda text: self.message.emit("error", text))
        self.device.connectedChanged.connect(self._on_connected)

    # ------------------------------------------------------------- session
    @property
    def session_open(self) -> bool:
        return self.recorder is not None

    def replace_session(self, session: Session) -> None:
        """Loads subject/experiment/configuration as a new draft session."""
        if self.session_open:
            raise RuntimeError("Cerrá la sesión actual antes de cargar otra.")
        session = copy.deepcopy(session)
        session.created = session.closed = ""
        self.session = session
        self.sessionChanged.emit()

    def open_session(self, root: Path) -> Path:
        if self.session_open:
            raise RuntimeError("Ya hay una sesión abierta.")
        if not self.session.subject.code.strip():
            raise ValueError("Falta el código del sujeto.")
        self.session.app_version = __version__
        self.session.firmware = self.device.info.get("FW", "")
        self.recorder = SessionRecorder(root, self.session)
        self.sessionChanged.emit()
        return self.recorder.folder

    def close_session(self) -> Optional[Path]:
        if not self.session_open:
            return None
        self.stop()
        self._save_timer.stop()
        folder = self.recorder.folder
        self.recorder.close()
        self.recorder = None
        # next session starts from a copy of this one
        self.session = copy.deepcopy(self.session)
        self.session.created = self.session.closed = ""
        self.sessionChanged.emit()
        return folder

    def session_modified(self) -> None:
        """Forms call this after editing the session (saved with a delay)."""
        if self.session_open:
            self._save_timer.start()

    def _save_now(self) -> None:
        if self.recorder is not None:
            self.recorder.save()

    def log_event(self, kind: str, text: str) -> None:
        if self.recorder is not None:
            self.recorder.event(kind, text)

    # -------------------------------------------------------- stimulation
    def check_ready(self, mode: str) -> list[str]:
        problems = []
        if not self.device.connected:
            problems.append("No hay conexión con la placa.")
        if not self.session_open:
            problems.append("Iniciá una sesión antes de estimular (todo queda registrado).")
        if self.state != "idle":
            problems.append("Ya hay una estimulación en curso.")
        config = self.session.config
        if config.ds8r.mode != "Monofásico positivo":
            problems.append("El DS8R debe estar en monofásico positivo (la polaridad la genera el puente).")
        problems += units.check_timing(config.timing, config.ds8r)
        if mode != "threshold":
            problems += units.check_channels(config)
            if not any(ch.enabled for ch in config.channels):
                problems.append("No hay canales habilitados.")
        if mode == "protocol":
            problems += runner.validate(config.protocol, config)
        return problems

    def _start(self, mode: str, enabled: list[bool], description: str) -> bool:
        problems = self.check_ready(mode)
        if problems:
            self.message.emit("error", "\n".join(problems))
            return False
        config = self.session.config
        commands = fwproto.config_commands(config) + [
            f"WDOG {WATCHDOG_MS}",
            fwproto.ens_command(enabled),
            fwproto.amps_command([0] * CHANNELS),
            "MON 1",
            "RUN",
        ]
        self._sent_permille = [0] * CHANNELS
        self._set_state(mode)

        def started(errors: list[str]) -> None:
            if self.state != mode or self.recorder is None:
                self.device.emergency_stop()        # stopped while configuring
                return
            if errors:
                self.device.emergency_stop()
                self._finish(f"No se pudo iniciar: {'; '.join(errors)}", "error")
                return
            run = self.recorder.start_run()
            self.log_event("run", f"Inicio {description} (corrida {run})")
            self._keepalive.start()
            if mode == "protocol":
                self._run_start = time.monotonic()
                self._protocol_timer.start()
            elif mode == "threshold":
                self._threshold_timer.start()
                self._send_amplitudes()
                self.thresholdLevel.emit(self.threshold_level())
            else:
                self._send_amplitudes()

        self.device.send_all(commands, started)
        return True

    def start_manual(self) -> bool:
        enabled = [ch.enabled for ch in self.session.config.channels]
        return self._start("manual", enabled, "control manual")

    def start_protocol(self) -> bool:
        config = self.session.config
        problems = self.check_ready("protocol")
        if problems:
            self.message.emit("error", "\n".join(problems))
            return False
        self._run = runner.ProtocolRun(runner.expand(config.protocol, config.channels))
        enabled = [ch.enabled for ch in config.channels]
        return self._start("protocol", enabled, f"protocolo '{config.protocol.name}' "
                                                f"({self._run.total_s:.1f} s)")

    def start_threshold(self, channel: int, start_ma: float, step_ma: float, interval_s: float,
                        cap_ma: float) -> bool:
        ds8r = self.session.config.ds8r
        self._threshold_channel = channel
        self._threshold_level = max(0.0, start_ma)
        self._threshold_step = max(0.0, step_ma)
        self._threshold_cap = max(0.0, min(cap_ma, ds8r.range_ma, ds8r.current_limit_ma))
        self._threshold_timer.setInterval(max(200, int(interval_s * 1000)))
        enabled = [i == channel for i in range(CHANNELS)]
        return self._start("threshold", enabled, f"rampa de umbrales canal {channel} "
                                                 f"(paso {step_ma} mA cada {interval_s} s, tope {self._threshold_cap} mA)")

    def stop(self, reason: str = "Detenido por el operador") -> None:
        if self.device.connected:
            self.device.emergency_stop()
        if self.state != "idle":
            self._finish(reason)

    def _finish(self, reason: str, level: str = "info") -> None:
        self._keepalive.stop()
        self._protocol_timer.stop()
        self._threshold_timer.stop()
        self._run = None
        self._threshold_channel = -1
        if self.device.connected:
            self.device.send("WDOG 0")
        self.log_event("stop", reason)
        self._set_state("idle")
        self.amplitudesSent.emit([0.0] * CHANNELS)
        self.message.emit(level, reason)

    def _set_state(self, state: str) -> None:
        self.state = state
        self.stateChanged.emit(state)

    def channels_changed(self) -> None:
        """Channel table edited: enable flags and polarities can change while
        stimulating (limits are applied on the next amplitude update)."""
        self.session_modified()
        if self.state not in ("manual", "protocol") or not self.device.connected:
            return
        channels = self.session.config.channels
        for ch in channels:
            self.device.send(f"CH {ch.index} POL {ch.polarity}")
        self.device.send(fwproto.ens_command([ch.enabled for ch in channels]))
        self._send_amplitudes()

    # ------------------------------------------------------ amplitudes
    def set_manual_amplitude(self, channel: int, ma: float) -> None:
        self._manual_ma[channel] = ma
        if self.state == "manual":
            self._send_amplitudes()

    def manual_amplitudes(self) -> list[float]:
        return list(self._manual_ma)

    def channel_max(self, channel: int) -> float:
        return units.channel_max_ma(self.session.config.channels[channel], self.session.config.ds8r)

    def _target_ma(self) -> list[float]:
        if self.state == "manual":
            values = list(self._manual_ma)
        elif self.state == "protocol" and self._run is not None:
            values, _ = self._run.at(time.monotonic() - self._run_start)
        elif self.state == "threshold":
            values = [0.0] * CHANNELS
            values[self._threshold_channel] = min(self._threshold_level, self._threshold_cap)
            return values
        else:
            values = [0.0] * CHANNELS
        return [min(max(0.0, v), self.channel_max(i)) for i, v in enumerate(values)]

    def _send_amplitudes(self) -> None:
        ds8r = self.session.config.ds8r
        target = self._target_ma()
        permille = [units.ma_to_permille(v, ds8r) for v in target]
        if permille != self._sent_permille:
            self._sent_permille = permille
            self.device.send(fwproto.amps_command(permille))
            self.amplitudesSent.emit([units.permille_to_ma(p, ds8r) for p in permille])

    def _protocol_tick(self) -> None:
        if self._run is None:
            return
        t = time.monotonic() - self._run_start
        _, index = self._run.at(t)
        if self._run.finished(t):
            total = self._run.total_s
            self.stop("Protocolo terminado")
            self.protocolProgress.emit(total, total, -1)
            return
        self._send_amplitudes()
        self.protocolProgress.emit(t, self._run.total_s, self._run.segments[index].block)

    def _threshold_tick(self) -> None:
        if self._threshold_level >= self._threshold_cap:
            self.stop("Rampa de umbrales: se alcanzó el tope sin marcar el máximo tolerado")
            return
        self._threshold_level = min(self._threshold_cap, self._threshold_level + self._threshold_step)
        self._send_amplitudes()
        self.thresholdLevel.emit(self._threshold_level)

    def threshold_level(self) -> float:
        return min(self._threshold_level, self._threshold_cap)

    def mark_threshold(self, kind: str) -> Optional[float]:
        """kind: sensory / motor / max. Stores the current ramp amplitude."""
        if self.state != "threshold":
            return None
        level = self.threshold_level()
        ch = self.session.config.channels[self._threshold_channel]
        names = {"sensory": "sensitivo", "motor": "motor", "max": "máximo tolerado"}
        setattr(ch, f"threshold_{kind}_ma", round(level, 3))
        self.log_event("threshold", f"Canal {ch.index}: umbral {names[kind]} = {level:.2f} mA")
        self.session_modified()
        if kind == "max":
            self.stop(f"Canal {ch.index}: máximo tolerado marcado en {level:.2f} mA")
        return level

    # ------------------------------------------------------------ monitor
    def request_wave(self, channel: int) -> None:
        if not self.device.connected:
            self.message.emit("error", "No hay conexión con la placa.")
            return

        def answer(line: str) -> None:
            if not line.startswith("OK"):
                self.message.emit("warning", f"WAVE: {line}")

        self.device.send(f"WAVE {channel}" if channel >= 0 else "WAVE", answer)

    def _on_pulse(self, report, host_time: float) -> None:
        if self.recorder is not None:
            self.recorder.pulse(report, host_time)

    def _on_wave(self, wave, host_time: float) -> None:
        if self.recorder is not None:
            self.recorder.wave(wave, host_time)

    def _on_line(self, direction: str, line: str) -> None:
        if self.recorder is not None and line != "STATUS" and not (direction == "RX" and line.startswith("OK RUN=")):
            self.recorder.command(direction, line)

    def _on_event(self, line: str) -> None:
        self.log_event("board", line)
        if self.state == "idle":
            return
        if line.startswith("EVT WDOG"):
            self._finish("La placa se detuvo por watchdog (se perdió la comunicación)", "error")
        elif line.startswith("EVT STOP BUTTON"):
            self._finish("Detenido con el botón de la placa", "warning")
        elif line.startswith("EVT DONE"):
            self._finish("La placa terminó la corrida")

    def _on_connected(self, connected: bool) -> None:
        if not connected and self.state != "idle":
            self._finish("Se perdió la conexión con la placa", "error")

    def connect_port(self, port: str) -> None:
        def ready(info: dict) -> None:
            fw = info.get("FW", "?")
            sim = " (simulador)" if info.get("SIM") else ""
            if fw != FIRMWARE_VERSION:
                self.message.emit("warning", f"Firmware {fw}{sim}: la aplicación espera la versión "
                                             f"{FIRMWARE_VERSION}.")
            else:
                self.message.emit("info", f"Conectado: firmware {fw}{sim}")
            self.session.firmware = fw
            self.log_event("connect", f"Conectado a {port}, firmware {fw}{sim}")

        self.device.connect_port(port, ready)

    def disconnect_port(self) -> None:
        self.stop("Desconexión")
        self.device.disconnect_port()
