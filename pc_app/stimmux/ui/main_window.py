"""Main window."""
from __future__ import annotations

import os
import sys
from pathlib import Path

from PySide6.QtCore import QSettings, Qt
from PySide6.QtGui import QAction, QKeySequence, QShortcut
from PySide6.QtWidgets import (QApplication, QComboBox, QFileDialog, QLabel, QLineEdit, QMainWindow, QMessageBox,
                               QPushButton, QSizePolicy, QTabWidget, QToolBar, QWidget)

from .. import __version__, units
from ..controller import Controller
from ..model import StimConfig, Subject, load_json, save_json
from ..storage import export_hdf5, load_session
from ..transport import list_ports
from .channels import ChannelsTab
from .forms import ExperimentTab, SubjectTab
from .monitor import LogTab, MonitorTab
from .stimulation import StimulationTab

STATE_NAMES = {"idle": "Detenido", "manual": "ESTIMULANDO (manual)", "protocol": "ESTIMULANDO (protocolo)",
               "threshold": "ESTIMULANDO (umbrales)"}
DATA_ROOT = Path.home() / "Documents" / "StimMux"


class MainWindow(QMainWindow):
    def __init__(self) -> None:
        super().__init__()
        self.setWindowTitle(f"StimMux {__version__}")
        # STIMMUX_SETTINGS lets the tests use their own settings
        self.settings = QSettings("UNER", os.environ.get("STIMMUX_SETTINGS", "StimMux"))
        self.ctrl = Controller(self)

        self.subject_tab = SubjectTab(self.ctrl, self._load_subject, self._save_subject)
        self.experiment_tab = ExperimentTab(self.ctrl)
        self.channels_tab = ChannelsTab(self.ctrl)
        self.stimulation_tab = StimulationTab(self.ctrl, self.confirm_start)
        self.monitor_tab = MonitorTab(self.ctrl)
        self.log_tab = LogTab(self.ctrl)
        self.tabs = QTabWidget()
        for widget, title in ((self.subject_tab, "Sujeto"), (self.experiment_tab, "Experimento y DS8R"),
                              (self.channels_tab, "Canales"), (self.stimulation_tab, "Estimulación"),
                              (self.monitor_tab, "Monitor"), (self.log_tab, "Registro")):
            self.tabs.addTab(widget, title)
        self.tabs.currentChanged.connect(lambda _: self._refresh_current())
        self.setCentralWidget(self.tabs)

        self._build_toolbar()
        self._build_menus()
        self.state_label = QLabel()
        self.statusBar().addPermanentWidget(self.state_label)

        stop = QShortcut(QKeySequence(Qt.Key_Escape), self)
        stop.setContext(Qt.ApplicationShortcut)
        stop.activated.connect(lambda: self.ctrl.stop("Detenido con Esc"))

        self.ctrl.stateChanged.connect(self._state)
        self.ctrl.sessionChanged.connect(self._session_changed)
        self.ctrl.message.connect(self._message)
        self.ctrl.device.connectedChanged.connect(self._connected)
        self._state("idle")
        self._session_changed()
        self._connected(False)
        self.resize(1300, 850)

    # -------------------------------------------------------------- UI
    def _build_toolbar(self) -> None:
        bar = QToolBar("Principal")
        bar.setMovable(False)
        self.addToolBar(bar)
        self.ports = QComboBox()
        self.ports.setMinimumWidth(220)
        refresh = QPushButton("↻")
        refresh.setToolTip("Actualizar puertos")
        refresh.clicked.connect(self._refresh_ports)
        self.connect_button = QPushButton()
        self.connect_button.clicked.connect(self._toggle_connection)
        self.session_button = QPushButton()
        self.session_button.clicked.connect(self._toggle_session)
        self.session_label = QLabel()
        self.marker = QLineEdit()
        self.marker.setPlaceholderText("Marcador (Ctrl+M)")
        self.marker.setMaximumWidth(220)
        self.marker.returnPressed.connect(self._add_marker)
        mark = QPushButton("Marcar")
        mark.clicked.connect(self._add_marker)
        QShortcut(QKeySequence("Ctrl+M"), self, activated=self.marker.setFocus)
        self.stop_button = QPushButton("DETENER (Esc)")
        self.stop_button.setStyleSheet("background: #c62828; color: white; font-weight: bold; padding: 6px 18px")
        self.stop_button.clicked.connect(lambda: self.ctrl.stop())

        for w in (self.ports, refresh, self.connect_button):
            bar.addWidget(w)
        bar.addSeparator()
        bar.addWidget(self.session_button)
        bar.addWidget(self.session_label)
        spacer = QWidget()
        spacer.setSizePolicy(QSizePolicy.Expanding, QSizePolicy.Preferred)
        bar.addWidget(spacer)
        bar.addWidget(self.marker)
        bar.addWidget(mark)
        bar.addSeparator()
        bar.addWidget(self.stop_button)
        self._refresh_ports()

    def _build_menus(self) -> None:
        menu = self.menuBar().addMenu("&Archivo")
        actions = [
            ("Cargar configuración…", self._load_config),
            ("Guardar configuración…", self._save_config),
            (None, None),
            ("Cargar sesión anterior…", self._load_previous_session),
            ("Exportar sesión a HDF5…", self._export_hdf5),
            (None, None),
            ("Carpeta de datos…", self._choose_root),
            (None, None),
            ("Salir", self.close),
        ]
        for text, slot in actions:
            if text is None:
                menu.addSeparator()
                continue
            action = QAction(text, self)
            action.triggered.connect(slot)
            menu.addAction(action)
        help_menu = self.menuBar().addMenu("A&yuda")
        about = QAction("Acerca de", self)
        about.triggered.connect(lambda: QMessageBox.about(
            self, "StimMux", f"StimMux {__version__}\nMultiplexor de estímulos DS8R + PWD5F60 (STM32U575)."))
        help_menu.addAction(about)

    def _refresh_current(self) -> None:
        widget = self.tabs.currentWidget()
        if hasattr(widget, "refresh"):
            widget.refresh()

    def _refresh_all(self) -> None:
        for tab in (self.subject_tab, self.experiment_tab, self.channels_tab, self.stimulation_tab):
            tab.refresh()

    # ------------------------------------------------------------ folders
    def _root(self) -> Path:
        return Path(self.settings.value("data_root", str(DATA_ROOT)))

    def _folder(self, name: str) -> Path:
        folder = self._root() / name
        folder.mkdir(parents=True, exist_ok=True)
        return folder

    def _choose_root(self) -> None:
        folder = QFileDialog.getExistingDirectory(self, "Carpeta de datos", str(self._root()))
        if folder:
            self.settings.setValue("data_root", folder)

    # --------------------------------------------------------- connection
    def _refresh_ports(self) -> None:
        current = self.ports.currentData() or self.settings.value("port", "")
        self.ports.clear()
        for device, description in list_ports():
            self.ports.addItem(f"{device} — {description}" if device != description else device, device)
        index = self.ports.findData(current)
        if index >= 0:
            self.ports.setCurrentIndex(index)

    def _toggle_connection(self) -> None:
        if self.ctrl.device.connected:
            self.ctrl.disconnect_port()
            return
        port = self.ports.currentData()
        try:
            self.ctrl.connect_port(port)
            self.settings.setValue("port", port)
        except Exception as exc:
            QMessageBox.critical(self, "Conexión", f"No se pudo abrir {port}:\n{exc}")

    def _connected(self, connected: bool) -> None:
        self.connect_button.setText("Desconectar" if connected else "Conectar")
        self.ports.setEnabled(not connected)

    # ------------------------------------------------------------ session
    def _toggle_session(self) -> None:
        if self.ctrl.session_open:
            if self.ctrl.state != "idle" and QMessageBox.question(
                    self, "Cerrar sesión", "Hay una estimulación en curso. ¿Detenerla y cerrar la sesión?") \
                    != QMessageBox.Yes:
                return
            folder = self.ctrl.close_session()
            self.statusBar().showMessage(f"Sesión guardada en {folder}", 10000)
            return
        session = self.ctrl.session
        if not session.experiment.consent_obtained and QMessageBox.warning(
                self, "Consentimiento", "No está marcado el consentimiento informado. ¿Iniciar la sesión igual?",
                QMessageBox.Yes | QMessageBox.No) != QMessageBox.Yes:
            return
        try:
            folder = self.ctrl.open_session(self._folder("sesiones"))
        except (ValueError, RuntimeError, OSError) as exc:
            QMessageBox.warning(self, "Sesión", str(exc))
            self.tabs.setCurrentWidget(self.subject_tab)
            return
        self.statusBar().showMessage(f"Sesión abierta: {folder}", 10000)

    def _session_changed(self) -> None:
        if self.ctrl.session_open:
            self.session_button.setText("Cerrar sesión")
            self.session_label.setText(f"  Grabando: {self.ctrl.recorder.folder.name}  ")
            self.session_label.setStyleSheet("color: #b00020; font-weight: bold")
        else:
            self.session_button.setText("Iniciar sesión")
            self.session_label.setText("  Sin sesión abierta  ")
            self.session_label.setStyleSheet("color: gray")
        self._refresh_all()

    def _add_marker(self) -> None:
        text = self.marker.text().strip()
        if not text:
            return
        if not self.ctrl.session_open:
            QMessageBox.information(self, "Marcador", "Los marcadores se guardan en la sesión: iniciá una sesión.")
            return
        self.ctrl.log_event("marker", text)
        self.statusBar().showMessage(f"Marcador: {text}", 5000)
        self.marker.clear()

    def confirm_start(self, description: str) -> bool:
        config = self.ctrl.session.config
        if config.ds8r.mode != "Monofásico positivo":
            QMessageBox.critical(self, "DS8R", "El DS8R debe estar configurado en monofásico positivo "
                                               "(Experimento y DS8R).")
            return False
        problems = self.ctrl.check_ready("manual" if "manual" in description else "protocol")
        if problems:
            QMessageBox.warning(self, "No se puede iniciar", "\n".join(problems))
            return False
        channels = [f"  canal {c.index} {c.label} {c.target} {c.side}: límite "
                    f"{units.channel_max_ma(c, config.ds8r):.2f} mA, {c.polarity}"
                    for c in config.channels if c.enabled]
        text = (f"Iniciar {description}\n\nSujeto: {self.ctrl.session.subject.code}\n"
                f"DS8R: rango {config.ds8r.range_ma} mA, ancho {config.ds8r.pulse_width_us} µs, "
                f"{config.ds8r.mode}\nCanales habilitados:\n" + "\n".join(channels))
        if not self.ctrl.session.experiment.consent_obtained:
            text += "\n\n⚠ No está marcado el consentimiento informado."
        return QMessageBox.question(self, "Confirmar estimulación", text) == QMessageBox.Yes

    # ------------------------------------------------------- load / save
    def _ask_open(self, title: str, folder: Path, pattern: str) -> Path | None:
        path, _ = QFileDialog.getOpenFileName(self, title, str(folder), pattern)
        return Path(path) if path else None

    def _ask_save(self, title: str, folder: Path, name: str, pattern: str) -> Path | None:
        path, _ = QFileDialog.getSaveFileName(self, title, str(folder / name), pattern)
        return Path(path) if path else None

    def _editable(self) -> bool:
        if self.ctrl.session_open:
            QMessageBox.information(self, "Sesión abierta", "Cerrá la sesión antes de cargar otros datos.")
            return False
        return True

    def _load_subject(self) -> None:
        if not self._editable():
            return
        path = self._ask_open("Cargar sujeto", self._folder("sujetos"), "Sujeto (*.json)")
        if path:
            try:
                self.ctrl.session.subject = load_json(path, Subject, "subject")
            except (ValueError, OSError) as exc:
                QMessageBox.warning(self, "Sujeto", str(exc))
            self._refresh_all()

    def _save_subject(self) -> None:
        subject = self.ctrl.session.subject
        path = self._ask_save("Guardar sujeto", self._folder("sujetos"), f"{subject.code or 'sujeto'}.json",
                              "Sujeto (*.json)")
        if path:
            save_json(path, "subject", subject)

    def _load_config(self) -> None:
        if not self._editable():
            return
        path = self._ask_open("Cargar configuración", self._folder("configuraciones"),
                              "Configuración (*.stimcfg.json)")
        if path:
            try:
                self.ctrl.session.config = load_json(path, StimConfig, "stimcfg")
            except (ValueError, OSError) as exc:
                QMessageBox.warning(self, "Configuración", str(exc))
            self._refresh_all()

    def _save_config(self) -> None:
        name = (self.ctrl.session.config.protocol.name or "configuracion").replace(" ", "_")
        path = self._ask_save("Guardar configuración", self._folder("configuraciones"), f"{name}.stimcfg.json",
                              "Configuración (*.stimcfg.json)")
        if path:
            if not path.name.endswith(".stimcfg.json"):
                path = path.with_name(path.name.removesuffix(".json") + ".stimcfg.json")
            save_json(path, "stimcfg", self.ctrl.session.config)

    def _load_previous_session(self) -> None:
        if not self._editable():
            return
        path = self._ask_open("Cargar sesión anterior", self._folder("sesiones"), "Sesión (session.json)")
        if not path:
            return
        try:
            session = load_session(path)
        except (ValueError, OSError) as exc:
            QMessageBox.warning(self, "Sesión", str(exc))
            return
        self.ctrl.replace_session(session)
        QMessageBox.information(self, "Sesión", "Se cargaron sujeto, experimento y configuración. "
                                                "Al iniciar la sesión se graba en una carpeta nueva.")

    def _export_hdf5(self) -> None:
        folder = QFileDialog.getExistingDirectory(self, "Carpeta de la sesión", str(self._folder("sesiones")))
        if not folder:
            return
        if self.ctrl.session_open and Path(folder) == self.ctrl.recorder.folder:
            QMessageBox.information(self, "HDF5", "Cerrá la sesión antes de exportarla.")
            return
        try:
            target = export_hdf5(Path(folder))
        except Exception as exc:
            QMessageBox.warning(self, "HDF5", f"No se pudo exportar: {exc}")
            return
        QMessageBox.information(self, "HDF5", f"Exportado en {target}")

    # -------------------------------------------------------------- state
    def _state(self, state: str) -> None:
        self.state_label.setText(STATE_NAMES.get(state, state))
        self.state_label.setStyleSheet("color: #b00020; font-weight: bold" if state != "idle" else "")
        self.session_button.setEnabled(state == "idle")

    def _message(self, level: str, text: str) -> None:
        self.statusBar().showMessage(text, 10000)
        if level == "error":
            QMessageBox.warning(self, "Atención", text)

    def closeEvent(self, event) -> None:
        if self.ctrl.state != "idle" and QMessageBox.question(
                self, "Salir", "Hay una estimulación en curso. ¿Detenerla y salir?") != QMessageBox.Yes:
            event.ignore()
            return
        self.ctrl.stop("Cierre de la aplicación")
        self.ctrl.close_session()
        self.ctrl.disconnect_port()
        event.accept()


def main() -> int:
    app = QApplication(sys.argv)
    app.setApplicationName("StimMux")
    window = MainWindow()
    window.show()
    return app.exec()
