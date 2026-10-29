"""Stimulation tab: timing, manual control and protocol by blocks."""
from __future__ import annotations

from PySide6.QtCore import Qt
from PySide6.QtWidgets import (QComboBox, QDoubleSpinBox, QFormLayout, QGridLayout, QGroupBox, QHBoxLayout,
                               QHeaderView, QLabel, QLineEdit, QMessageBox, QProgressBar, QPushButton, QSlider,
                               QTableWidget, QTableWidgetItem, QVBoxLayout, QWidget)

from .. import runner, units
from ..controller import Controller
from ..model import BLOCK_KINDS, BLOCK_NAMES, CHANNELS, UNIT_NAMES, UNITS, ProtocolBlock
from .binder import Binder

SLIDER_STEPS = 1000


def format_values(values: list[float]) -> str:
    if all(v == values[0] for v in values):
        return f"{values[0]:g}"
    return "; ".join(f"{v:g}" for v in values)


def parse_values(text: str) -> list[float]:
    """'10' -> every channel; '10; 5,5; 0' -> per channel (missing = 0).
    Values are separated by ';' or spaces, the decimal separator can be ',' or '.'."""
    parts = text.replace(";", " ").split()
    values = [float(p.replace(",", ".")) for p in parts]
    if not values:
        return [0.0] * CHANNELS
    if len(values) == 1:
        return values * CHANNELS
    if len(values) > CHANNELS:
        raise ValueError(f"hay {len(values)} valores y solo {CHANNELS} canales")
    return values + [0.0] * (CHANNELS - len(values))


class TimingBox(QGroupBox):
    def __init__(self, ctrl: Controller) -> None:
        super().__init__("Temporización (se envía a la placa al iniciar)")
        self.ctrl = ctrl
        self.binder = Binder(lambda: ctrl.session.config.timing, self._changed)
        b = self.binder
        self.frequency = QDoubleSpinBox(decimals=2, minimum=0.1, maximum=1000, suffix=" Hz")
        self.frequency.valueChanged.connect(self._frequency)
        self.sequence = QLineEdit()
        self.sequence.setPlaceholderText("canales separados por espacios, p. ej. 0 1 2 3")
        self.sequence.editingFinished.connect(self._sequence)
        self.pulse = QLabel()
        self.summary = QLabel()
        self.summary.setWordWrap(True)

        form = QFormLayout(self)
        form.addRow("Frecuencia de frame", self.frequency)
        form.addRow("Secuencia de slots", self.sequence)
        form.addRow("Ancho de pulso (DS8R)", self.pulse)
        advanced = QHBoxLayout()
        for attr, text in (("settle_us", "settle"), ("trigger_us", "trigger"), ("ds8r_delay_us", "retardo DS8R"),
                           ("tail_us", "cola")):
            advanced.addWidget(QLabel(text))
            advanced.addWidget(b.integer(attr, 5 if attr != "ds8r_delay_us" else 0, 5000, "µs"))
        form.addRow("Avanzado", advanced)
        form.addRow(self.summary)
        self.refresh()

    def refresh(self) -> None:
        t = self.ctrl.session.config.timing
        self.binder.refresh()
        self.frequency.blockSignals(True)
        self.frequency.setValue(1e6 / t.period_us)
        self.frequency.blockSignals(False)
        self.sequence.setText(" ".join(str(c) for c in t.sequence))
        self._update_summary()

    def _changed(self) -> None:
        self.ctrl.session_modified()
        self._update_summary()

    def _frequency(self, hz: float) -> None:
        self.ctrl.session.config.timing.period_us = int(round(1e6 / hz))
        self._changed()

    def _sequence(self) -> None:
        try:
            seq = [int(p) for p in self.sequence.text().replace(",", " ").split()]
            if not seq or any(not 0 <= c < CHANNELS for c in seq):
                raise ValueError
        except ValueError:
            QMessageBox.warning(self, "Secuencia", f"La secuencia debe ser una lista de canales 0..{CHANNELS - 1}.")
            self.refresh()
            return
        self.ctrl.session.config.timing.sequence = seq
        self._changed()

    def _update_summary(self) -> None:
        config = self.ctrl.session.config
        t = config.timing
        self.pulse.setText(f"{config.ds8r.pulse_width_us} µs (se cambia en Experimento → DS8R)")
        rates = [f"canal {c}: {t.channel_rate_hz(c):g} Hz" for c in sorted(set(t.sequence))]
        errors = units.check_timing(t, config.ds8r)
        text = f"Slot de {t.period_us // max(1, len(t.sequence))} µs. " + ", ".join(rates) + "."
        if errors:
            text += "\n⚠ " + "\n⚠ ".join(errors)
        self.summary.setText(text)
        self.summary.setStyleSheet("color: #b00020" if errors else "")


class ManualBox(QGroupBox):
    def __init__(self, ctrl: Controller) -> None:
        super().__init__("Control manual (mA, limitado por el límite de seguridad de cada canal)")
        self.ctrl = ctrl
        grid = QGridLayout(self)
        self.names, self.spins, self.sliders, self.sent = [], [], [], []
        for ch in range(CHANNELS):
            name = QLabel()
            spin = QDoubleSpinBox(decimals=2, suffix=" mA", singleStep=0.1)
            slider = QSlider(Qt.Horizontal)
            slider.setRange(0, SLIDER_STEPS)
            sent = QLabel("0.00 mA")
            sent.setMinimumWidth(80)
            spin.valueChanged.connect(lambda v, c=ch: self._spin(c, v))
            slider.valueChanged.connect(lambda v, c=ch: self._slider(c, v))
            for col, w in enumerate((name, spin, slider, sent)):
                grid.addWidget(w, ch, col)
            self.names.append(name)
            self.spins.append(spin)
            self.sliders.append(slider)
            self.sent.append(sent)
        grid.setColumnStretch(2, 1)
        self.start = QPushButton("Iniciar control manual")
        self.start.clicked.connect(self._start)
        zero = QPushButton("Todo a 0")
        zero.clicked.connect(lambda: [s.setValue(0) for s in self.spins])
        grid.addWidget(self.start, CHANNELS, 0, 1, 2)
        grid.addWidget(zero, CHANNELS, 2)
        ctrl.amplitudesSent.connect(self._sent)
        self.refresh()

    def refresh(self) -> None:
        values = self.ctrl.manual_amplitudes()
        for ch, channel in enumerate(self.ctrl.session.config.channels):
            maximum = self.ctrl.channel_max(ch)
            self.names[ch].setText(f"{ch}  {channel.label}  {channel.target}".strip())
            for w in (self.spins[ch], self.sliders[ch]):
                w.blockSignals(True)
            self.spins[ch].setMaximum(maximum)
            self.spins[ch].setValue(min(values[ch], maximum))
            self.sliders[ch].setValue(int(round(self.spins[ch].value() / maximum * SLIDER_STEPS)) if maximum else 0)
            for w in (self.spins[ch], self.sliders[ch]):
                w.blockSignals(False)
                w.setEnabled(maximum > 0)
            if self.spins[ch].value() != values[ch]:
                self.ctrl.set_manual_amplitude(ch, self.spins[ch].value())

    def _spin(self, ch: int, value: float) -> None:
        maximum = self.spins[ch].maximum()
        self.sliders[ch].blockSignals(True)
        self.sliders[ch].setValue(int(round(value / maximum * SLIDER_STEPS)) if maximum else 0)
        self.sliders[ch].blockSignals(False)
        self.ctrl.set_manual_amplitude(ch, value)

    def _slider(self, ch: int, position: int) -> None:
        self.spins[ch].setValue(position / SLIDER_STEPS * self.spins[ch].maximum())

    def _sent(self, values: list) -> None:
        for label, value in zip(self.sent, values):
            label.setText(f"{value:.2f} mA")

    def _start(self) -> None:
        if self.confirm("control manual"):
            self.ctrl.start_manual()

    def set_state(self, state: str) -> None:
        self.start.setEnabled(state == "idle")


BLOCK_COLUMNS = ["Tipo", "Duración (s)", "Unidad", "Inicio", "Fin", "Repetir bloques", "Veces"]


class ProtocolBox(QGroupBox):
    def __init__(self, ctrl: Controller) -> None:
        super().__init__("Protocolo por bloques")
        self.ctrl = ctrl
        self._loading = False
        self.binder = Binder(lambda: ctrl.session.config.protocol, ctrl.session_modified)
        self.table = QTableWidget(0, len(BLOCK_COLUMNS))
        self.table.setHorizontalHeaderLabels(BLOCK_COLUMNS)
        self.table.horizontalHeader().setSectionResizeMode(QHeaderView.Stretch)
        self.table.horizontalHeader().setSectionResizeMode(2, QHeaderView.ResizeToContents)
        self.table.itemChanged.connect(self._item_changed)

        buttons = QHBoxLayout()
        for text, slot in (("Agregar", self._add), ("Quitar", self._remove), ("Subir", lambda: self._move(-1)),
                           ("Bajar", lambda: self._move(1)), ("Validar", self._validate)):
            button = QPushButton(text)
            button.clicked.connect(slot)
            buttons.addWidget(button)
        buttons.addStretch()
        self.run = QPushButton("Ejecutar protocolo")
        self.run.clicked.connect(self._run)
        buttons.addWidget(self.run)

        self.progress = QProgressBar()
        self.progress.setFormat("%v / %m s")
        self.status = QLabel()
        help_text = QLabel("Inicio/Fin: un valor para todos los canales o una lista por canal separada por ';' "
                           "(p. ej. '20; 35,5; 0'). "
                           "'Repetir' vuelve a ejecutar los N bloques anteriores la cantidad de veces indicada.")
        help_text.setWordWrap(True)
        help_text.setStyleSheet("color: gray")

        form = QFormLayout()
        form.addRow("Nombre", self.binder.line("name"))
        layout = QVBoxLayout(self)
        layout.addLayout(form)
        layout.addWidget(self.table)
        layout.addLayout(buttons)
        layout.addWidget(help_text)
        layout.addWidget(self.progress)
        layout.addWidget(self.status)
        ctrl.protocolProgress.connect(self._progress)
        self.refresh()

    # ---------------------------------------------------------------- table
    @property
    def blocks(self) -> list[ProtocolBlock]:
        return self.ctrl.session.config.protocol.blocks

    def refresh(self) -> None:
        self.binder.refresh()
        self._loading = True
        try:
            self.table.setRowCount(len(self.blocks))
            for row, block in enumerate(self.blocks):
                kind = self.table.cellWidget(row, 0) or self._combo(row, 0, "kind",
                                                                    [(k, BLOCK_NAMES[k]) for k in BLOCK_KINDS])
                unit = self.table.cellWidget(row, 2) or self._combo(row, 2, "unit",
                                                                    [(u, UNIT_NAMES[u]) for u in UNITS])
                for combo, value in ((kind, block.kind), (unit, block.unit)):
                    combo.blockSignals(True)
                    combo.setCurrentIndex(combo.findData(value))
                    combo.blockSignals(False)
                texts = {1: f"{block.duration_s:g}", 3: format_values(block.start), 4: format_values(block.end),
                         5: str(block.repeat_span), 6: str(block.repeat_count)}
                used = {"ramp": {1, 2, 3, 4}, "hold": {1, 2, 3}, "rest": {1}, "repeat": {5, 6}}[block.kind]
                unit.setEnabled(2 in used)
                for col, text in texts.items():
                    item = QTableWidgetItem(text if col in used else "")
                    if col not in used:
                        item.setFlags(Qt.NoItemFlags)
                    self.table.setItem(row, col, item)
        finally:
            self._loading = False
        self._update_total()

    def _combo(self, row: int, col: int, attr: str, items: list[tuple[str, str]]) -> QComboBox:
        """Created once per row; refresh() only changes the selection."""
        combo = QComboBox()
        for value, text in items:
            combo.addItem(text, value)
        combo.currentIndexChanged.connect(lambda _: self._set(row, attr, combo.currentData()))
        self.table.setCellWidget(row, col, combo)
        return combo

    def _item_changed(self, item: QTableWidgetItem) -> None:
        if self._loading:
            return
        row, col = item.row(), item.column()
        block = self.blocks[row]
        text = item.text().strip()
        try:
            if col == 1:
                block.duration_s = float(text.replace(",", "."))
            elif col == 3:
                block.start = parse_values(text)
            elif col == 4:
                block.end = parse_values(text)
            elif col == 5:
                block.repeat_span = int(text)
            elif col == 6:
                block.repeat_count = int(text)
        except ValueError as exc:
            QMessageBox.warning(self, "Valor inválido", f"Bloque {row + 1}, {BLOCK_COLUMNS[col]}: {exc}")
        self.ctrl.session_modified()
        self.refresh()

    def _set(self, row: int, attr: str, value) -> None:
        setattr(self.blocks[row], attr, value)
        self.ctrl.session_modified()
        self.refresh()

    def _selected(self) -> int:
        return self.table.currentRow()

    def _add(self) -> None:
        row = self._selected()
        block = ProtocolBlock("hold", 5.0, "percent", [100.0] * CHANNELS, [100.0] * CHANNELS)
        self.blocks.insert(row + 1 if row >= 0 else len(self.blocks), block)
        self.ctrl.session_modified()
        self.refresh()

    def _remove(self) -> None:
        row = self._selected()
        if 0 <= row < len(self.blocks):
            del self.blocks[row]
            self.ctrl.session_modified()
            self.refresh()

    def _move(self, delta: int) -> None:
        row = self._selected()
        target = row + delta
        if 0 <= row < len(self.blocks) and 0 <= target < len(self.blocks):
            self.blocks[row], self.blocks[target] = self.blocks[target], self.blocks[row]
            self.ctrl.session_modified()
            self.refresh()
            self.table.setCurrentCell(target, 0)

    def _update_total(self) -> None:
        config = self.ctrl.session.config
        try:
            total = runner.total_duration(runner.expand(config.protocol, config.channels))
            self.status.setText(f"Duración total: {total:.1f} s")
        except ValueError as exc:
            self.status.setText(str(exc))

    def _validate(self) -> bool:
        config = self.ctrl.session.config
        errors = runner.validate(config.protocol, config) + units.check_channels(config)
        if errors:
            QMessageBox.warning(self, "Protocolo", "\n".join(errors))
            return False
        QMessageBox.information(self, "Protocolo", "El protocolo respeta los límites de todos los canales.")
        return True

    def _run(self) -> None:
        if self.confirm(f"el protocolo '{self.ctrl.session.config.protocol.name}'"):
            self.ctrl.start_protocol()

    def _progress(self, t: float, total: float, block: int) -> None:
        self.progress.setMaximum(max(1, int(round(total))))
        self.progress.setValue(int(t))
        if block >= 0:
            self.status.setText(f"Bloque {block + 1}: {BLOCK_NAMES[self.blocks[block].kind]}")

    def set_state(self, state: str) -> None:
        self.run.setEnabled(state == "idle")
        self.table.setEnabled(state != "protocol")
        if state == "idle":
            self._update_total()


class StimulationTab(QWidget):
    def __init__(self, ctrl: Controller, confirm_start) -> None:
        super().__init__()
        self.timing = TimingBox(ctrl)
        self.manual = ManualBox(ctrl)
        self.protocol = ProtocolBox(ctrl)
        self.manual.confirm = confirm_start
        self.protocol.confirm = confirm_start
        left = QVBoxLayout()
        left.addWidget(self.timing)
        left.addWidget(self.manual)
        left.addStretch()
        layout = QHBoxLayout(self)
        layout.addLayout(left, 1)
        layout.addWidget(self.protocol, 1)
        ctrl.stateChanged.connect(self.set_state)
        self.set_state(ctrl.state)

    def refresh(self) -> None:
        self.timing.refresh()
        self.manual.refresh()
        self.protocol.refresh()

    def set_state(self, state: str) -> None:
        self.timing.setEnabled(state == "idle")
        self.manual.set_state(state)
        self.protocol.set_state(state)
