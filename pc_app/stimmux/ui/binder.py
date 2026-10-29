"""Two-way binding between form widgets and dataclass attributes.

The bound object is obtained through a function, because the session can be
replaced (loaded) at any time; refresh() reloads every widget from it.
"""
from __future__ import annotations

from typing import Any, Callable, Optional

from PySide6.QtGui import QDoubleValidator, QIntValidator
from PySide6.QtWidgets import (QCheckBox, QComboBox, QDoubleSpinBox, QLineEdit, QPlainTextEdit, QSpinBox,
                               QWidget)


def parse_optional_float(text: str) -> Optional[float]:
    text = text.strip().replace(",", ".")
    if not text:
        return None
    return float(text)


class Binder:
    def __init__(self, target: Callable[[], Any], changed: Callable[[], None]) -> None:
        self.target = target
        self.changed = changed
        self._refreshers: list[Callable[[], None]] = []

    def _set(self, attr: str, value: Any) -> None:
        setattr(self.target(), attr, value)
        self.changed()

    def refresh(self) -> None:
        for refresh in self._refreshers:
            refresh()

    def _add(self, widget: QWidget, refresh: Callable[[], None]) -> None:
        def guarded() -> None:
            widget.blockSignals(True)
            try:
                refresh()
            finally:
                widget.blockSignals(False)
        self._refreshers.append(guarded)
        guarded()

    # ------------------------------------------------------------ widgets
    def line(self, attr: str, placeholder: str = "") -> QLineEdit:
        w = QLineEdit()
        w.setPlaceholderText(placeholder)
        w.textEdited.connect(lambda text: self._set(attr, text))
        self._add(w, lambda: w.setText(getattr(self.target(), attr) or ""))
        return w

    def text(self, attr: str) -> QPlainTextEdit:
        w = QPlainTextEdit()
        w.setMaximumHeight(90)
        w.textChanged.connect(lambda: self._set(attr, w.toPlainText()))
        self._add(w, lambda: w.setPlainText(getattr(self.target(), attr) or ""))
        return w

    def optional_number(self, attr: str, integer: bool = False, placeholder: str = "") -> QLineEdit:
        """Empty field = None."""
        w = QLineEdit()
        w.setPlaceholderText(placeholder)
        w.setValidator(QIntValidator(0, 10_000_000) if integer else QDoubleValidator(0.0, 1e9, 3))

        def edited(text: str) -> None:
            try:
                value = parse_optional_float(text)
            except ValueError:
                return
            if integer and value is not None:
                value = int(value)
            self._set(attr, value)

        def refresh() -> None:
            value = getattr(self.target(), attr)
            w.setText("" if value is None else (str(int(value)) if integer else f"{value:g}"))

        w.textEdited.connect(edited)
        self._add(w, refresh)
        return w

    def number(self, attr: str, minimum: float, maximum: float, decimals: int = 2,
               suffix: str = "", step: float = 1.0) -> QDoubleSpinBox:
        w = QDoubleSpinBox()
        w.setRange(minimum, maximum)
        w.setDecimals(decimals)
        w.setSingleStep(step)
        if suffix:
            w.setSuffix(" " + suffix)
        w.valueChanged.connect(lambda v: self._set(attr, float(v)))
        self._add(w, lambda: w.setValue(float(getattr(self.target(), attr))))
        return w

    def integer(self, attr: str, minimum: int, maximum: int, suffix: str = "") -> QSpinBox:
        w = QSpinBox()
        w.setRange(minimum, maximum)
        if suffix:
            w.setSuffix(" " + suffix)
        w.valueChanged.connect(lambda v: self._set(attr, int(v)))
        self._add(w, lambda: w.setValue(int(getattr(self.target(), attr))))
        return w

    def combo(self, attr: str, items: list[tuple[str, str]], editable: bool = False) -> QComboBox:
        """items: [(stored value, displayed text)]"""
        w = QComboBox()
        w.setEditable(editable)
        for value, text in items:
            w.addItem(text, value)

        def chosen() -> None:
            value = w.currentData()
            self._set(attr, w.currentText() if value is None else value)

        def refresh() -> None:
            value = getattr(self.target(), attr)
            index = w.findData(value)
            if index >= 0:
                w.setCurrentIndex(index)
            elif editable:
                w.setEditText(str(value))

        w.currentIndexChanged.connect(lambda _: chosen())
        if editable:
            w.lineEdit().textEdited.connect(lambda text: self._set(attr, text))
        self._add(w, refresh)
        return w

    def check(self, attr: str, text: str) -> QCheckBox:
        w = QCheckBox(text)
        w.toggled.connect(lambda on: self._set(attr, bool(on)))
        self._add(w, lambda: w.setChecked(bool(getattr(self.target(), attr))))
        return w
