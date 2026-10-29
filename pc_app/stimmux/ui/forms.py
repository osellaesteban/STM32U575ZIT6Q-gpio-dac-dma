"""Subject and experiment (including DS8R settings and calibration) forms."""
from __future__ import annotations

from PySide6.QtWidgets import (QFormLayout, QGroupBox, QHBoxLayout, QLabel, QPushButton, QVBoxLayout, QWidget)

from ..controller import Controller
from .binder import Binder

SEX = [("", "—"), ("F", "Femenino"), ("M", "Masculino"), ("X", "Otro / no informa")]
HANDEDNESS = [("", "—"), ("Diestro", "Diestro"), ("Zurdo", "Zurdo"), ("Ambidiestro", "Ambidiestro")]
DS8R_MODES = [("Monofásico positivo", "Monofásico positivo"),
              ("Monofásico negativo", "Monofásico negativo (NO usar)"),
              ("Bifásico", "Bifásico (NO usar)")]


class SubjectTab(QWidget):
    def __init__(self, ctrl: Controller, load_subject, save_subject) -> None:
        super().__init__()
        self.binder = Binder(lambda: ctrl.session.subject, ctrl.session_modified)
        b = self.binder
        form = QFormLayout()
        form.addRow("Código *", b.line("code", "p. ej. S012 (sin nombre ni documento)"))
        form.addRow("Año de nacimiento", b.optional_number("birth_year", integer=True))
        form.addRow("Sexo", b.combo("sex", SEX))
        form.addRow("Lateralidad", b.combo("handedness", HANDEDNESS))
        form.addRow("Peso (kg)", b.optional_number("weight_kg"))
        form.addRow("Talla (cm)", b.optional_number("height_cm"))
        form.addRow("Condición clínica", b.line("condition", "diagnóstico, nivel de lesión, etc."))
        form.addRow("Notas", b.text("notes"))

        buttons = QHBoxLayout()
        load = QPushButton("Cargar sujeto…")
        load.clicked.connect(load_subject)
        save = QPushButton("Guardar sujeto…")
        save.clicked.connect(save_subject)
        buttons.addWidget(load)
        buttons.addWidget(save)
        buttons.addStretch()

        note = QLabel("Los archivos de sesión identifican al sujeto solo por el código. "
                      "La correspondencia con la identidad real se guarda fuera de esta aplicación.")
        note.setWordWrap(True)
        note.setStyleSheet("color: gray")

        layout = QVBoxLayout(self)
        layout.addLayout(form)
        layout.addLayout(buttons)
        layout.addWidget(note)
        layout.addStretch()

    def refresh(self) -> None:
        self.binder.refresh()


class ExperimentTab(QWidget):
    def __init__(self, ctrl: Controller) -> None:
        super().__init__()
        self.ctrl = ctrl
        exp = Binder(lambda: ctrl.session.experiment, ctrl.session_modified)
        experiment = QGroupBox("Experimento")
        f = QFormLayout(experiment)
        f.addRow("Estudio", exp.line("study"))
        f.addRow("Protocolo", exp.line("protocol_name", "también se usa en el nombre de la carpeta"))
        f.addRow("Operador", exp.line("operator"))
        f.addRow("Código de aprobación ética", exp.line("ethics_code"))
        f.addRow("", exp.check("consent_obtained", "Consentimiento informado firmado"))
        f.addRow("Notas del consentimiento", exp.line("consent_notes"))
        f.addRow("Notas", exp.text("notes"))

        self.ds8r_binder = Binder(lambda: ctrl.session.config.ds8r, self._ds8r_changed)
        d = self.ds8r_binder
        ds8r = QGroupBox("DS8R (valores cargados a mano en el equipo)")
        g = QFormLayout(ds8r)
        g.addRow("Rango (corriente a 10 V)", d.number("range_ma", 0.1, 1000, 1, "mA"))
        g.addRow("Ancho de pulso total", d.integer("pulse_width_us", 50, 2000, "µs"))
        g.addRow("Modo", d.combo("mode", DS8R_MODES))
        g.addRow("Límite de corriente", d.number("current_limit_ma", 0.1, 1000, 1, "mA"))
        g.addRow("Notas", d.line("notes"))
        self.mode_warning = QLabel()
        self.mode_warning.setWordWrap(True)
        self.mode_warning.setStyleSheet("color: #b00020; font-weight: bold")
        g.addRow(self.mode_warning)

        cal_t = Binder(lambda: ctrl.session.config.timing, ctrl.session_modified)
        cal_c = Binder(lambda: ctrl.session.config.calibration, ctrl.session_modified)
        calibration = QGroupBox("Calibración")
        h = QFormLayout(calibration)
        h.addRow("Corriente medida por cuenta del ADC", cal_c.number("ma_per_count", 0, 100, 6, "mA", 0.0001))
        h.addRow("DACFS (cuenta del DAC = 10 V en el DS8R)", cal_t.integer("dac_full_scale", 1, 4095))
        h.addRow("Recorte de bordes por fase (monitor)", cal_t.integer("monitor_trim_us", 0, 500, "µs"))
        hint = QLabel("Con 'mA por cuenta' en 0 el monitor muestra cuentas del ADC sin convertir.")
        hint.setStyleSheet("color: gray")
        h.addRow(hint)
        self.binders = [exp, d, cal_t, cal_c]

        layout = QVBoxLayout(self)
        layout.addWidget(experiment)
        layout.addWidget(ds8r)
        layout.addWidget(calibration)
        layout.addStretch()
        self._update_warning()

    def _ds8r_changed(self) -> None:
        self.ctrl.session_modified()
        self._update_warning()

    def _update_warning(self) -> None:
        mode = self.ctrl.session.config.ds8r.mode
        self.mode_warning.setText("" if mode == "Monofásico positivo" else
                                  "El DS8R debe estar en monofásico positivo: la polaridad y el bifásico los "
                                  "genera el puente. En otros modos la fase negativa se desvía por los diodos "
                                  "de los puentes y no llega al electrodo.")

    def refresh(self) -> None:
        for binder in self.binders:
            binder.refresh()
        self._update_warning()
