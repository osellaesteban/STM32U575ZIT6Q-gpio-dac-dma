"""Channel table and the assisted threshold ramp."""
from __future__ import annotations

from PySide6.QtCore import Qt
from PySide6.QtWidgets import (QAbstractItemView, QComboBox, QDialog, QDoubleSpinBox, QFormLayout, QGridLayout,
                               QHBoxLayout, QHeaderView, QLabel, QMessageBox, QPushButton, QTableWidget,
                               QTableWidgetItem, QVBoxLayout, QWidget)

from ..controller import Controller
from ..model import CHANNELS, POLARITIES, POLARITY_NAMES
from .binder import parse_optional_float

SIDES = ["", "Derecho", "Izquierdo", "Bilateral"]

# (attribute, header, kind) kind: check / text / side / polarity / optional / number
COLUMNS = [
    ("enabled", "Hab.", "check"),
    ("label", "Etiqueta", "text"),
    ("target", "Músculo / nervio", "text"),
    ("side", "Lado", "side"),
    ("polarity", "Polaridad", "polarity"),
    ("electrode_type", "Electrodo (tipo)", "text"),
    ("electrode_size", "Tamaño", "text"),
    ("electrode_location", "Ubicación", "text"),
    ("impedance_kohm", "Impedancia kΩ", "optional"),
    ("threshold_sensory_ma", "U. sensitivo mA", "optional"),
    ("threshold_motor_ma", "U. motor mA", "optional"),
    ("threshold_max_ma", "Máx. tolerado mA", "optional"),
    ("working_level_ma", "Nivel trabajo mA", "number"),
    ("safety_limit_ma", "Límite mA", "number"),
]


class ChannelsTab(QWidget):
    def __init__(self, ctrl: Controller) -> None:
        super().__init__()
        self.ctrl = ctrl
        self._loading = False
        self.table = QTableWidget(CHANNELS, len(COLUMNS))
        self.table.setHorizontalHeaderLabels([c[1] for c in COLUMNS])
        self.table.setVerticalHeaderLabels([f"Canal {i}" for i in range(CHANNELS)])
        self.table.horizontalHeader().setSectionResizeMode(QHeaderView.ResizeToContents)
        self.table.setSelectionBehavior(QAbstractItemView.SelectRows)
        self.table.setSelectionMode(QAbstractItemView.SingleSelection)
        self.table.itemChanged.connect(self._item_changed)

        threshold = QPushButton("Medir umbrales del canal seleccionado…")
        threshold.clicked.connect(self._thresholds)
        suggest = QPushButton("Límite = máximo tolerado")
        suggest.setToolTip("Copia el máximo tolerado medido como límite de seguridad del canal seleccionado")
        suggest.clicked.connect(self._limit_from_max)
        buttons = QHBoxLayout()
        buttons.addWidget(threshold)
        buttons.addWidget(suggest)
        buttons.addStretch()

        note = QLabel("Solo los canales habilitados con límite de seguridad mayor que 0 pueden estimular. "
                      "Los bloques del protocolo en '%' usan el nivel de trabajo de cada canal.")
        note.setWordWrap(True)
        note.setStyleSheet("color: gray")
        layout = QVBoxLayout(self)
        layout.addWidget(self.table)
        layout.addLayout(buttons)
        layout.addWidget(note)
        self.refresh()

    def refresh(self) -> None:
        self._loading = True
        try:
            for row, ch in enumerate(self.ctrl.session.config.channels):
                for col, (attr, _, kind) in enumerate(COLUMNS):
                    value = getattr(ch, attr)
                    if kind == "check":
                        item = QTableWidgetItem()
                        item.setFlags(Qt.ItemIsUserCheckable | Qt.ItemIsEnabled | Qt.ItemIsSelectable)
                        item.setCheckState(Qt.Checked if value else Qt.Unchecked)
                        self.table.setItem(row, col, item)
                    elif kind in ("side", "polarity"):
                        combo = self.table.cellWidget(row, col) or self._combo(row, col, attr, kind)
                        combo.blockSignals(True)
                        combo.setCurrentIndex(max(0, combo.findData(value)))
                        combo.blockSignals(False)
                    else:
                        text = "" if value is None else (f"{value:g}" if isinstance(value, float) else str(value))
                        self.table.setItem(row, col, QTableWidgetItem(text))
        finally:
            self._loading = False

    def _combo(self, row: int, col: int, attr: str, kind: str) -> QComboBox:
        """Created once per cell; refresh() only changes the selection."""
        combo = QComboBox()
        if kind == "side":
            for side in SIDES:
                combo.addItem(side or "—", side)
        else:
            for pol in POLARITIES:
                combo.addItem(POLARITY_NAMES[pol], pol)
        combo.currentIndexChanged.connect(lambda _: self._set(row, attr, combo.currentData()))
        self.table.setCellWidget(row, col, combo)
        return combo

    def _item_changed(self, item: QTableWidgetItem) -> None:
        if self._loading:
            return
        row, col = item.row(), item.column()
        attr, header, kind = COLUMNS[col]
        if kind == "check":
            self._set(row, attr, item.checkState() == Qt.Checked)
            return
        text = item.text().strip()
        if kind == "text":
            self._set(row, attr, text)
            return
        try:
            value = parse_optional_float(text)
            if value is not None and value < 0:
                raise ValueError
        except ValueError:
            QMessageBox.warning(self, "Valor inválido", f"{header}: '{text}' no es un número válido.")
            self.refresh()
            return
        if kind == "number":
            value = value or 0.0
        self._set(row, attr, value)

    def _set(self, row: int, attr: str, value) -> None:
        setattr(self.ctrl.session.config.channels[row], attr, value)
        self.ctrl.channels_changed()

    def _selected(self) -> int:
        rows = self.table.selectionModel().selectedRows()
        return rows[0].row() if rows else -1

    def _thresholds(self) -> None:
        row = self._selected()
        if row < 0:
            QMessageBox.information(self, "Umbrales", "Seleccioná un canal en la tabla.")
            return
        ThresholdDialog(self.ctrl, row, self).exec()
        self.refresh()

    def _limit_from_max(self) -> None:
        row = self._selected()
        if row < 0:
            return
        ch = self.ctrl.session.config.channels[row]
        if ch.threshold_max_ma is None:
            QMessageBox.information(self, "Límite", "El canal no tiene máximo tolerado medido.")
            return
        ch.safety_limit_ma = ch.threshold_max_ma
        self.ctrl.log_event("limit", f"Canal {row}: límite = máximo tolerado ({ch.threshold_max_ma} mA)")
        self.ctrl.channels_changed()
        self.refresh()


class ThresholdDialog(QDialog):
    """Step ramp on one channel; the operator marks the thresholds."""

    def __init__(self, ctrl: Controller, channel: int, parent=None) -> None:
        super().__init__(parent)
        self.ctrl = ctrl
        self.channel = channel
        ch = ctrl.session.config.channels[channel]
        ds8r = ctrl.session.config.ds8r
        self.setWindowTitle(f"Umbrales — canal {channel} ({ch.label} {ch.target} {ch.side})".strip())

        self.start = QDoubleSpinBox(suffix=" mA", decimals=2, maximum=1000, value=0.0)
        self.step = QDoubleSpinBox(suffix=" mA", decimals=2, minimum=0.05, maximum=50, value=0.5, singleStep=0.1)
        self.interval = QDoubleSpinBox(suffix=" s", decimals=1, minimum=0.2, maximum=30, value=2.0)
        cap = ch.safety_limit_ma if ch.safety_limit_ma > 0 else min(ds8r.range_ma, ds8r.current_limit_ma)
        self.cap = QDoubleSpinBox(suffix=" mA", decimals=2, minimum=0.1, maximum=min(ds8r.range_ma, ds8r.current_limit_ma),
                                  value=cap)
        form = QFormLayout()
        form.addRow("Amplitud inicial", self.start)
        form.addRow("Paso", self.step)
        form.addRow("Cada", self.interval)
        form.addRow("Tope de la rampa", self.cap)

        self.level = QLabel("—")
        self.level.setStyleSheet("font-size: 28px; font-weight: bold")
        self.level.setAlignment(Qt.AlignCenter)
        self.results = QLabel()

        self.start_button = QPushButton("Iniciar rampa")
        self.start_button.clicked.connect(self._start)
        stop = QPushButton("DETENER")
        stop.setStyleSheet("background: #c62828; color: white; font-weight: bold")
        stop.clicked.connect(lambda: self.ctrl.stop("Rampa de umbrales detenida por el operador"))
        self.marks = []
        grid = QGridLayout()
        for i, (kind, text) in enumerate((("sensory", "Sensitivo"), ("motor", "Motor"),
                                          ("max", "Máximo tolerado"))):
            button = QPushButton(text)
            button.setMinimumHeight(40)
            button.clicked.connect(lambda _=False, k=kind: self._mark(k))
            grid.addWidget(button, 0, i)
            self.marks.append(button)
        grid.addWidget(self.start_button, 1, 0, 1, 2)
        grid.addWidget(stop, 1, 2)

        layout = QVBoxLayout(self)
        layout.addLayout(form)
        layout.addWidget(self.level)
        layout.addLayout(grid)
        layout.addWidget(self.results)

        ctrl.thresholdLevel.connect(self._level)
        ctrl.stateChanged.connect(self._state)
        self._state(ctrl.state)
        self._show_results()

    def _start(self) -> None:
        self.ctrl.start_threshold(self.channel, self.start.value(), self.step.value(), self.interval.value(),
                                  self.cap.value())

    def _mark(self, kind: str) -> None:
        if self.ctrl.mark_threshold(kind) is not None:
            self._show_results()

    def _level(self, value: float) -> None:
        self.level.setText(f"{value:.2f} mA")

    def _state(self, state: str) -> None:
        running = state == "threshold"
        self.start_button.setEnabled(state == "idle")
        for button in self.marks:
            button.setEnabled(running)
        if not running:
            self.level.setText("—")

    def _show_results(self) -> None:
        ch = self.ctrl.session.config.channels[self.channel]

        def fmt(v):
            return "—" if v is None else f"{v:.2f} mA"
        self.results.setText(f"Sensitivo: {fmt(ch.threshold_sensory_ma)}    Motor: {fmt(ch.threshold_motor_ma)}    "
                             f"Máximo tolerado: {fmt(ch.threshold_max_ma)}")

    def done(self, result: int) -> None:
        if self.ctrl.state == "threshold":
            self.ctrl.stop("Rampa de umbrales cerrada")
        self.ctrl.thresholdLevel.disconnect(self._level)
        self.ctrl.stateChanged.disconnect(self._state)
        super().done(result)
