"""Current monitor tab and communication log."""
from __future__ import annotations

import time
from collections import deque

import pyqtgraph as pg
from PySide6.QtCore import QTimer
from PySide6.QtWidgets import (QComboBox, QHBoxLayout, QHeaderView, QLabel, QPlainTextEdit, QPushButton, QSplitter,
                               QTableWidget, QTableWidgetItem, QVBoxLayout, QWidget)
from PySide6.QtCore import Qt

from .. import units
from ..controller import Controller
from ..model import CHANNELS

HISTORY = 5000
COLORS = ["#1f77b4", "#ff7f0e", "#2ca02c", "#d62728", "#9467bd", "#8c564b", "#e377c2", "#7f7f7f"]
TABLE_COLUMNS = ["Pedido mA", "Media", "Mediana", "Mín.", "Máx.", "Media fase 2", "Pulsos"]


class MonitorTab(QWidget):
    def __init__(self, ctrl: Controller) -> None:
        super().__init__()
        self.ctrl = ctrl
        self.t0 = time.monotonic()
        self.times = [deque(maxlen=HISTORY) for _ in range(CHANNELS)]
        self.values = [deque(maxlen=HISTORY) for _ in range(CHANNELS)]
        self.counts = [0] * CHANNELS
        self._dirty = False

        self.status = QLabel("Sin datos")
        self.window = QComboBox()
        for seconds in (10, 30, 60, 300):
            self.window.addItem(f"últimos {seconds} s", seconds)
        self.window.setCurrentIndex(1)
        clear = QPushButton("Limpiar")
        clear.clicked.connect(self.clear)
        top = QHBoxLayout()
        top.addWidget(self.status, 1)
        top.addWidget(self.window)
        top.addWidget(clear)

        self.plot = pg.PlotWidget()
        self.plot.setBackground("w")
        self.plot.showGrid(x=True, y=True, alpha=0.3)
        self.plot.setLabel("bottom", "tiempo de sesión", "s")
        self.legend = self.plot.addLegend(offset=(-10, 10))
        self.curves = [self.plot.plot([], [], pen=None, symbol="o", symbolSize=4, symbolBrush=COLORS[i],
                                      symbolPen=None) for i in range(CHANNELS)]
        self._in_legend = [False] * CHANNELS

        self.table = QTableWidget(CHANNELS, len(TABLE_COLUMNS))
        self.table.setHorizontalHeaderLabels(TABLE_COLUMNS)
        self.table.setVerticalHeaderLabels([f"Canal {i}" for i in range(CHANNELS)])
        self.table.horizontalHeader().setSectionResizeMode(QHeaderView.Stretch)
        self.table.setEditTriggers(QTableWidget.NoEditTriggers)

        self.wave_channel = QComboBox()
        self.wave_channel.addItem("cualquier canal", -1)
        for i in range(CHANNELS):
            self.wave_channel.addItem(f"canal {i}", i)
        capture = QPushButton("Capturar forma de onda del próximo pulso")
        capture.clicked.connect(lambda: ctrl.request_wave(self.wave_channel.currentData()))
        self.wave_info = QLabel()
        wave_bar = QHBoxLayout()
        wave_bar.addWidget(self.wave_channel)
        wave_bar.addWidget(capture)
        wave_bar.addWidget(self.wave_info, 1)
        self.wave_plot = pg.PlotWidget()
        self.wave_plot.setBackground("w")
        self.wave_plot.showGrid(x=True, y=True, alpha=0.3)
        self.wave_plot.setLabel("bottom", "tiempo desde el cierre del puente", "µs")
        self.wave_plot.setLabel("left", "cuentas del ADC")
        wave_box = QWidget()
        wave_layout = QVBoxLayout(wave_box)
        wave_layout.setContentsMargins(0, 0, 0, 0)
        wave_layout.addLayout(wave_bar)
        wave_layout.addWidget(self.wave_plot)

        splitter = QSplitter(Qt.Vertical)
        splitter.addWidget(self.plot)
        splitter.addWidget(self.table)
        splitter.addWidget(wave_box)
        layout = QVBoxLayout(self)
        layout.addLayout(top)
        layout.addWidget(splitter)

        ctrl.device.pulseReceived.connect(self._pulse)
        ctrl.device.waveReceived.connect(self._wave)
        ctrl.statusUpdated.connect(self._status)
        self._timer = QTimer(self)
        self._timer.setInterval(200)
        self._timer.timeout.connect(self._redraw)
        self._timer.start()
        self._update_axis()

    def clear(self) -> None:
        self.t0 = time.monotonic()
        for d in self.times + self.values:
            d.clear()
        self.counts = [0] * CHANNELS
        self.legend.clear()
        self._in_legend = [False] * CHANNELS
        self._dirty = True

    def _calibration(self) -> float:
        return self.ctrl.session.config.calibration.ma_per_count

    def _update_axis(self) -> None:
        self.plot.setLabel("left", "corriente medida (media fase 1)", "mA" if self._calibration() > 0 else "cuentas")

    def _convert(self, counts: float, baseline: float) -> float:
        value = units.counts_to_ma(counts, baseline, self._calibration())
        return counts - baseline if value is None else value

    def _pulse(self, report, host_time: float) -> None:
        ch = report.channel
        if not 0 <= ch < CHANNELS or not report.phases:
            return
        p1 = report.phases[0]
        if not self._in_legend[ch]:         # only channels with data
            self._in_legend[ch] = True
            self.legend.addItem(self.curves[ch], f"canal {ch} {self.ctrl.session.config.channels[ch].label}")
        self.times[ch].append(host_time - self.t0)
        self.values[ch].append(self._convert(p1.mean, report.baseline))
        self.counts[ch] += 1
        row = [units.permille_to_ma(report.amplitude, self.ctrl.session.config.ds8r)]
        row += [self._convert(v, report.baseline) for v in (p1.mean, p1.median, p1.min, p1.max)]
        row.append(self._convert(report.phases[1].mean, report.baseline) if len(report.phases) > 1 else None)
        row.append(self.counts[ch])
        for col, value in enumerate(row):
            text = "" if value is None else (str(value) if isinstance(value, int) else f"{value:.2f}")
            self.table.setItem(ch, col, QTableWidgetItem(text))
        self._dirty = True

    def _redraw(self) -> None:
        if not self._dirty:
            return
        self._dirty = False
        self._update_axis()
        span = self.window.currentData()
        latest = max((t[-1] for t in self.times if t), default=0.0)
        for curve, t, v in zip(self.curves, self.times, self.values):
            curve.setData(list(t), list(v))
        self.plot.setXRange(max(0.0, latest - span), max(span, latest), padding=0)

    def _status(self, status: dict) -> None:
        sync = status.get("MSYNC") == "1"
        self.status.setText(f"Frames: {status.get('FRAMES', '?')}   Pulsos sin analizar: {status.get('MLOST', '?')}   "
                            f"Reportes descartados: {status.get('TXDROP', '?')}   "
                            + ("⚠ monitor desincronizado o sin ADC" if sync else "Monitor OK"))
        self.status.setStyleSheet("color: #b00020" if sync else "")

    def _wave(self, wave, host_time: float) -> None:
        self.wave_plot.clear()
        x = [i * wave.ts_us for i in range(len(wave.samples))]
        self.wave_plot.plot(x, wave.samples, pen=pg.mkPen(COLORS[wave.channel % len(COLORS)], width=2))
        marks = [("trigger", wave.i_trig, "#888"), ("inicio", wave.i_phase1, "#2ca02c"), ("fin", wave.i_end, "#d62728")]
        if wave.biphasic:
            marks.append(("inversión", wave.i_flip, "#9467bd"))
        for name, index, color in marks:
            line = pg.InfiniteLine(index * wave.ts_us, angle=90, pen=pg.mkPen(color, style=Qt.DashLine),
                                   label=name, labelOpts={"position": 0.9, "color": color})
            self.wave_plot.addItem(line)
        self.wave_info.setText(f"Canal {wave.channel}, frame {wave.frame}, amplitud {wave.amplitude} ‰, "
                               f"{'bifásico' if wave.biphasic else 'monofásico'}, primera fase "
                               f"{'+' if wave.positive else '-'}, {wave.n} muestras")


class LogTab(QWidget):
    MAX_LINES = 2000

    def __init__(self, ctrl: Controller) -> None:
        super().__init__()
        self.text = QPlainTextEdit()
        self.text.setReadOnly(True)
        self.text.setMaximumBlockCount(self.MAX_LINES)
        self.hide_status = QPushButton("Ocultar STATUS")
        self.hide_status.setCheckable(True)
        self.hide_status.setChecked(True)
        layout = QVBoxLayout(self)
        layout.addWidget(self.hide_status)
        layout.addWidget(self.text)
        ctrl.device.lineLogged.connect(self._line)
        ctrl.message.connect(lambda level, text: self._line(level.upper(), text))

    def _line(self, direction: str, line: str) -> None:
        if self.hide_status.isChecked() and (line == "STATUS" or line.startswith("OK RUN=")):
            return
        self.text.appendPlainText(f"{time.strftime('%H:%M:%S')}  {direction:7s} {line}")
