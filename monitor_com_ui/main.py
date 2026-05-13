"""STM32 Serial Monitor - Main application entry point

This module contains the main window and UI setup for the serial monitor application.
The refactored code is split into modules for better maintainability:
  - models.py: Data classes and enums
  - parsers.py: Serial message parsing
  - formatters.py: Display formatting and colors
  - commands.py: Device command builders
  - serial_worker.py: Serial communication worker thread
  - styles.py: UI theme and styling
  - config.py: Application configuration constants
"""

import sys
import serial.tools.list_ports
from PyQt6.QtWidgets import (
    QApplication, QMainWindow, QWidget, QVBoxLayout, QHBoxLayout,
    QTabWidget, QTextEdit, QPushButton, QComboBox, QLabel, QStatusBar,
    QFrame, QCheckBox, QGridLayout, QLineEdit, QScrollArea, QTableWidget, QTableWidgetItem,
    QMessageBox
)
from PyQt6.QtGui import QIntValidator, QColor, QFont
from PyQt6.QtCore import QDateTime, QTimer, Qt

try:
    # When run as a module
    from .config import UIConfig, DeviceConfig
    from .serial_worker import SerialWorker
    from .parsers import TelemetryParser, SettingsParser
    from .commands import Command
    from .styles import DARK_STYLESHEET
    from .telemetry_table import TelemetryTableView
    from .logger import setup_logging
except ImportError:
    # When run directly
    from config import UIConfig, DeviceConfig
    from serial_worker import SerialWorker
    from parsers import TelemetryParser, SettingsParser
    from commands import Command
    from styles import DARK_STYLESHEET
    from telemetry_table import TelemetryTableView
    from logger import setup_logging

logger = setup_logging()


class SerialMonitorUI(QMainWindow):
    """Main application window for STM32 serial monitor"""

    def __init__(self):
        super().__init__()
        self.setWindowTitle("STM32 Serial Monitor")
        self.setGeometry(100, 100, UIConfig.WINDOW_WIDTH, UIConfig.WINDOW_HEIGHT)

        self.serial_worker = None
        self.autoscroll_enabled = True
        self.telemetry_view = None
        self.accumulated_settings = {}

        self.find_ports_timer = QTimer()
        self.find_ports_timer.timeout.connect(self.update_port_list)
        self.find_ports_timer.start(UIConfig.PORT_SCAN_INTERVAL)

        self.init_ui()
        self.update_port_list()

    def init_ui(self):
        """Initialize the user interface"""
        central_widget = QWidget()
        self.setCentralWidget(central_widget)

        main_layout = QHBoxLayout()
        main_layout.setContentsMargins(0, 0, 0, 0)
        main_layout.setSpacing(0)
        left_layout = QVBoxLayout()
        left_layout.setContentsMargins(10, 8, 10, 8)
        left_layout.setSpacing(6)

        # Connection control
        control_layout = QHBoxLayout()
        control_layout.addWidget(QLabel("Port:"))
        self.port_combo = QComboBox()
        control_layout.addWidget(self.port_combo)

        self.connect_btn = QPushButton("Connect")
        self.connect_btn.clicked.connect(self.toggle_connection)
        control_layout.addWidget(self.connect_btn)

        self.clear_btn = QPushButton("Clear")
        self.clear_btn.clicked.connect(self.clear_output)
        control_layout.addWidget(self.clear_btn)

        self.autoscroll_checkbox = QCheckBox("Autoscroll")
        self.autoscroll_checkbox.setChecked(True)
        self.autoscroll_checkbox.stateChanged.connect(self.on_autoscroll_toggled)
        control_layout.addWidget(self.autoscroll_checkbox)

        control_layout.addStretch()

        self.status_label = QLabel("● Disconnected")
        self.status_label.setStyleSheet("color: #e05555; font-weight: bold; letter-spacing: 0.5px;")
        control_layout.addWidget(self.status_label)

        left_layout.addLayout(control_layout)

        # Mode control
        mode_layout = QHBoxLayout()
        mode_layout.addWidget(QLabel("Mode:"))

        self.mode_normal_btn = QPushButton("Normal")
        self.mode_normal_btn.clicked.connect(lambda: self.send_command(Command.MODE_NORMAL))
        self.mode_normal_btn.setStyleSheet(
            "QPushButton { background-color: #1a4a8a; color: #7ec8f8; border: 1px solid #2d6abf; border-radius: 5px; font-weight: 700; padding: 5px 16px; }"
            "QPushButton:hover { background-color: #2055a0; }"
            "QPushButton:pressed { background-color: #153878; }"
            "QPushButton:disabled { background-color: #1e2128; color: #444; border-color: #252930; }"
        )
        self.mode_normal_btn.setEnabled(False)
        mode_layout.addWidget(self.mode_normal_btn)

        self.mode_manual_btn = QPushButton("Manual")
        self.mode_manual_btn.clicked.connect(lambda: self.send_command(Command.MODE_MANUAL))
        self.mode_manual_btn.setStyleSheet(
            "QPushButton { background-color: #7a1a1a; color: #f8a0a0; border: 1px solid #b03030; border-radius: 5px; font-weight: 700; padding: 5px 16px; }"
            "QPushButton:hover { background-color: #8a2020; }"
            "QPushButton:pressed { background-color: #5a1010; }"
            "QPushButton:disabled { background-color: #1e2128; color: #444; border-color: #252930; }"
        )
        self.mode_manual_btn.setEnabled(False)
        mode_layout.addWidget(self.mode_manual_btn)

        mode_layout.addStretch()
        left_layout.addLayout(mode_layout)

        # Tabs
        self.tabs = QTabWidget()

        self.raw_text = QTextEdit()
        self.raw_text.setReadOnly(True)
        font = QFont("Consolas", 9)
        self.raw_text.setFont(font)
        self.tabs.addTab(self.raw_text, "Raw Data")

        # Telemetry table
        telemetry_table = QTableWidget()
        telemetry_table.setColumnCount(15)
        telemetry_table.setHorizontalHeaderLabels([
            "Received", "External Sensor", "OnBoard Sensor", "RH(%)", "State",
            "Uptime", "FAN1", "FAN2", "FAN3", "FAN4", "In A(%)", "Out A(%)", "In B(%)", "Out B(%)", "Button"
        ])
        telemetry_table.setAlternatingRowColors(True)
        telemetry_table.verticalHeader().setVisible(False)
        telemetry_table.horizontalHeader().setStretchLastSection(True)

        self.telemetry_view = TelemetryTableView(telemetry_table)
        self.tabs.addTab(telemetry_table, "Telemetry")

        left_layout.addWidget(self.tabs)

        # Command input
        command_layout = QHBoxLayout()
        command_layout.addWidget(QLabel("Command:"))

        self.command_input = QLineEdit()
        self.command_input.setPlaceholderText("Type command and press Enter or click Send...")
        self.command_input.returnPressed.connect(self.send_custom_command)
        self.command_input.setEnabled(False)
        command_layout.addWidget(self.command_input)

        self.send_command_btn = QPushButton("Send")
        self.send_command_btn.setStyleSheet(
            "QPushButton { background-color: #3a1a5a; color: #c49ef8; border: 1px solid #6030a0; border-radius: 5px; font-weight: 700; }"
            "QPushButton:hover { background-color: #4a2070; }"
            "QPushButton:pressed { background-color: #2a1040; }"
            "QPushButton:disabled { background-color: #1e2128; color: #444; border-color: #252930; }"
        )
        self.send_command_btn.setFixedWidth(80)
        self.send_command_btn.setEnabled(False)
        self.send_command_btn.clicked.connect(self.send_custom_command)
        command_layout.addWidget(self.send_command_btn)

        left_layout.addLayout(command_layout)
        main_layout.addLayout(left_layout)

        # Right panel - Settings controls
        right_panel = QFrame()
        right_panel.setStyleSheet("QFrame { border-left: 1px solid #252930; background-color: #1a1d23; }")
        right_panel.setMinimumWidth(320)
        right_panel.setMaximumWidth(420)
        right_scroll = QVBoxLayout()
        right_scroll.setContentsMargins(0, 0, 0, 0)
        right_scroll.setSpacing(0)

        right_title = QLabel("Settings")
        right_title.setStyleSheet(
            "font-weight: 700; font-size: 13px; padding: 12px 16px 10px 16px; "
            "color: #c0c8d8; border-bottom: 1px solid #252930; background-color: #1e2128; letter-spacing: 1px;"
        )
        right_scroll.addWidget(right_title)

        # Settings frame
        settings_frame = QFrame()
        settings_frame.setStyleSheet("QFrame { border: none; background-color: #1a1d23; }")
        settings_layout = QVBoxLayout()
        settings_layout.setContentsMargins(16, 12, 16, 12)
        settings_layout.setSpacing(12)

        # Read settings button
        self.read_settings_btn = QPushButton("Read Current Settings")
        self.read_settings_btn.setStyleSheet(
            "QPushButton { background-color: #1a3d7a; color: #7ec8f8; border: 1px solid #2d6abf; border-radius: 5px; font-weight: 700; padding: 5px 14px; }"
            "QPushButton:hover { background-color: #2055a0; }"
            "QPushButton:pressed { background-color: #122d5a; }"
            "QPushButton:disabled { background-color: #1e2128; color: #444; border-color: #252930; }"
        )
        self.read_settings_btn.setEnabled(False)
        self.read_settings_btn.clicked.connect(lambda: self.send_command(Command.SETTINGS_READ))
        settings_layout.addWidget(self.read_settings_btn)

        # Settings display
        self.settings_display = QTextEdit()
        self.settings_display.setReadOnly(True)
        self.settings_display.setFont(QFont("Consolas", 9))
        self.settings_display.setMaximumHeight(200)
        settings_layout.addWidget(self.settings_display)

        # Temp Critical
        settings_layout.addWidget(QLabel("Temp Critical (2-90°C):"))
        self.ctrl_temp_critical = QLineEdit()
        self.ctrl_temp_critical.setValidator(QIntValidator(2, 90))
        self.ctrl_temp_critical.setMaxLength(3)
        self.ctrl_temp_critical.setEnabled(False)
        settings_layout.addWidget(self.ctrl_temp_critical)
        btn_critical = QPushButton("Set")
        btn_critical.setEnabled(False)
        btn_critical.clicked.connect(lambda: self.send_temp_setting("critical"))
        settings_layout.addWidget(btn_critical)
        self.ctrl_temp_critical_btn = btn_critical

        # Temp Throttle On
        settings_layout.addWidget(QLabel("Temp Throttle On (1-254°C):"))
        self.ctrl_temp_throttle = QLineEdit()
        self.ctrl_temp_throttle.setValidator(QIntValidator(1, 254))
        self.ctrl_temp_throttle.setMaxLength(3)
        self.ctrl_temp_throttle.setEnabled(False)
        settings_layout.addWidget(self.ctrl_temp_throttle)
        btn_throttle = QPushButton("Set")
        btn_throttle.setEnabled(False)
        btn_throttle.clicked.connect(lambda: self.send_temp_setting("throttle"))
        settings_layout.addWidget(btn_throttle)
        self.ctrl_temp_throttle_btn = btn_throttle

        # Temp Fan On
        settings_layout.addWidget(QLabel("Temp Fan On (1-80°C):"))
        self.ctrl_temp_fan_on = QLineEdit()
        self.ctrl_temp_fan_on.setValidator(QIntValidator(1, 80))
        self.ctrl_temp_fan_on.setMaxLength(3)
        self.ctrl_temp_fan_on.setEnabled(False)
        settings_layout.addWidget(self.ctrl_temp_fan_on)
        btn_fan_on = QPushButton("Set")
        btn_fan_on.setEnabled(False)
        btn_fan_on.clicked.connect(lambda: self.send_temp_setting("fan_on"))
        settings_layout.addWidget(btn_fan_on)
        self.ctrl_temp_fan_on_btn = btn_fan_on

        # Temp Fan Off
        settings_layout.addWidget(QLabel("Temp Fan Off (0-79°C):"))
        self.ctrl_temp_fan_off = QLineEdit()
        self.ctrl_temp_fan_off.setValidator(QIntValidator(0, 79))
        self.ctrl_temp_fan_off.setMaxLength(3)
        self.ctrl_temp_fan_off.setEnabled(False)
        settings_layout.addWidget(self.ctrl_temp_fan_off)
        btn_fan_off = QPushButton("Set")
        btn_fan_off.setEnabled(False)
        btn_fan_off.clicked.connect(lambda: self.send_temp_setting("fan_off"))
        settings_layout.addWidget(btn_fan_off)
        self.ctrl_temp_fan_off_btn = btn_fan_off

        # PWM Throttle A
        settings_layout.addWidget(QLabel("PWM Throttle A (0-100%):"))
        self.ctrl_pwm_a = QLineEdit()
        self.ctrl_pwm_a.setValidator(QIntValidator(0, 100))
        self.ctrl_pwm_a.setMaxLength(3)
        self.ctrl_pwm_a.setEnabled(False)
        settings_layout.addWidget(self.ctrl_pwm_a)
        btn_a = QPushButton("Set")
        btn_a.setEnabled(False)
        btn_a.clicked.connect(lambda: self.send_pwm_setting("A"))
        settings_layout.addWidget(btn_a)
        self.ctrl_pwm_a_btn = btn_a

        # PWM Throttle B
        settings_layout.addWidget(QLabel("PWM Throttle B (0-100%):"))
        self.ctrl_pwm_b = QLineEdit()
        self.ctrl_pwm_b.setValidator(QIntValidator(0, 100))
        self.ctrl_pwm_b.setMaxLength(3)
        self.ctrl_pwm_b.setEnabled(False)
        settings_layout.addWidget(self.ctrl_pwm_b)
        btn_b = QPushButton("Set")
        btn_b.setEnabled(False)
        btn_b.clicked.connect(lambda: self.send_pwm_setting("B"))
        settings_layout.addWidget(btn_b)
        self.ctrl_pwm_b_btn = btn_b

        settings_layout.addStretch()

        # Button layout for defaults
        button_layout = QHBoxLayout()
        button_layout.setSpacing(8)

        # Set Default button
        self.set_default_btn = QPushButton("Set to Current")
        self.set_default_btn.setStyleSheet(
            "QPushButton { background-color: #1a3d7a; color: #7ec8f8; border: 1px solid #2d6abf; border-radius: 5px; font-weight: 700; padding: 5px 14px; }"
            "QPushButton:hover { background-color: #2055a0; }"
            "QPushButton:pressed { background-color: #122d5a; }"
            "QPushButton:disabled { background-color: #1e2128; color: #444; border-color: #252930; }"
        )
        self.set_default_btn.setEnabled(False)
        self.set_default_btn.clicked.connect(lambda: self.confirm_and_send_default())
        button_layout.addWidget(self.set_default_btn)

        # Reset button
        self.reset_defaults_btn = QPushButton("Reset to Defaults")
        self.reset_defaults_btn.setStyleSheet(
            "QPushButton { background-color: #5a1a1a; color: #f88080; border: 1px solid #902020; border-radius: 5px; font-weight: 700; padding: 5px 14px; }"
            "QPushButton:hover { background-color: #6a2020; }"
            "QPushButton:pressed { background-color: #3a1010; }"
            "QPushButton:disabled { background-color: #1e2128; color: #444; border-color: #252930; }"
        )
        self.reset_defaults_btn.setEnabled(False)
        self.reset_defaults_btn.clicked.connect(lambda: self.confirm_and_send_reset())
        button_layout.addWidget(self.reset_defaults_btn)

        settings_layout.addLayout(button_layout)

        settings_frame.setLayout(settings_layout)
        scroll_area = QScrollArea()
        scroll_area.setWidgetResizable(True)
        scroll_area.setWidget(settings_frame)
        scroll_area.setStyleSheet("QScrollArea { background-color: #1a1d23; border: none; }")
        right_scroll.addWidget(scroll_area)

        right_panel.setLayout(right_scroll)
        main_layout.addWidget(right_panel)

        central_widget.setLayout(main_layout)
        self.setStatusBar(QStatusBar())
        self.statusBar().showMessage("Ready")

    def find_stm_port(self):
        """Find the STM32 device port by VID/PID"""
        for port in serial.tools.list_ports.comports():
            if port.vid == DeviceConfig.TINYUSB_VID and port.pid == DeviceConfig.TINYUSB_CDC_PID:
                return port.device
        return None

    def get_all_ports(self):
        """Get list of all available COM ports"""
        return [port.device for port in serial.tools.list_ports.comports()]

    def update_port_list(self):
        """Update the port combo box with available ports"""
        current = self.port_combo.currentText()
        ports = self.get_all_ports()

        if self.port_combo.count() != len(ports) or (len(ports) > 0 and current != self.port_combo.currentText()):
            self.port_combo.clear()
            self.port_combo.addItems(ports)

            stm_port = self.find_stm_port()
            if stm_port and stm_port in ports:
                self.port_combo.setCurrentText(stm_port)

    def toggle_connection(self):
        """Toggle between connect and disconnect"""
        if self.serial_worker is None:
            self.connect()
        else:
            self.disconnect()

    def connect(self):
        """Establish serial connection"""
        port = self.port_combo.currentText()
        if not port:
            self.status_label.setText("● No port selected")
            self.status_label.setStyleSheet("color: #e05555; font-weight: bold; letter-spacing: 0.5px;")
            return

        self.serial_worker = SerialWorker(port)
        self.serial_worker.data_received.connect(self.on_data_received)
        self.serial_worker.status_changed.connect(self.on_status_changed)
        self.serial_worker.connection_state.connect(self.on_connection_state)
        self.serial_worker.finished.connect(self.on_worker_finished)
        self.serial_worker.start()
        self.status_label.setText("● Connecting...")
        self.status_label.setStyleSheet("color: #ffc107; font-weight: bold; letter-spacing: 0.5px;")

    def disconnect(self):
        """Close serial connection"""
        if self.serial_worker:
            logger.info("Disconnect requested - stopping serial worker thread")
            self.serial_worker.stop()
            self.status_label.setText("● Disconnecting...")
            self.status_label.setStyleSheet("color: #ffc107; font-weight: bold; letter-spacing: 0.5px;")
            self.on_connection_state(False)

    def on_worker_finished(self):
        """Handle worker thread completion - ensure UI is updated"""
        logger.info("Serial worker thread has finished")
        self.serial_worker = None
        self.on_connection_state(False)

    def on_data_received(self, data):
        """Handle incoming serial data"""
        if not self.serial_worker:
            return

        self.raw_text.insertPlainText(data)

        # Parse telemetry
        if data.startswith('$01,'):
            telemetry = TelemetryParser.parse(data.rstrip('\r\n'))
            if telemetry and self.telemetry_view:
                self.telemetry_view.add_row(telemetry)

        # Parse settings
        settings = SettingsParser.parse_settings_from_text(data)
        if settings:
            self.update_settings_display(settings)

        self.autoscroll_to_bottom()

    def update_settings_display(self, settings: dict):
        """Update settings display from parsed settings dict"""
        self.accumulated_settings.update(settings)

        display_text = ""
        for key, value in sorted(self.accumulated_settings.items()):
            display_text += f"{key} = {value}\n"
        self.settings_display.setPlainText(display_text)

        if 'PWM_THROTTLE_A' in self.accumulated_settings:
            self.ctrl_pwm_a.setText(self.accumulated_settings['PWM_THROTTLE_A'])
        if 'PWM_THROTTLE_B' in self.accumulated_settings:
            self.ctrl_pwm_b.setText(self.accumulated_settings['PWM_THROTTLE_B'])
        if 'TEMP_THROTTLE_ON' in self.accumulated_settings:
            self.ctrl_temp_throttle.setText(self.accumulated_settings['TEMP_THROTTLE_ON'])
        if 'TEMP_FAN_ON' in self.accumulated_settings:
            self.ctrl_temp_fan_on.setText(self.accumulated_settings['TEMP_FAN_ON'])
        if 'TEMP_FAN_OFF' in self.accumulated_settings:
            self.ctrl_temp_fan_off.setText(self.accumulated_settings['TEMP_FAN_OFF'])
        if 'TEMP_CRITICAL' in self.accumulated_settings:
            self.ctrl_temp_critical.setText(self.accumulated_settings['TEMP_CRITICAL'])

        logger.info(f"Accumulated settings: {self.accumulated_settings}")

    def send_pwm_setting(self, channel: str):
        """Send PWM throttle setting"""
        if channel == "A":
            value = self.ctrl_pwm_a.text().strip()
        else:
            value = self.ctrl_pwm_b.text().strip()

        if not value or not value.isdigit() or int(value) > 100:
            self.statusBar().showMessage(f"PWM {channel}: Enter a value between 0-100")
            return

        reply = QMessageBox.question(self, "Confirm Setting",
            f"Set PWM Throttle {channel} to {value}%?",
            QMessageBox.StandardButton.Yes | QMessageBox.StandardButton.No)

        if reply == QMessageBox.StandardButton.Yes:
            self.send_command(Command.pwm_throttle(channel, int(value)))

    def send_temp_setting(self, setting_type: str):
        """Send temperature setting"""
        if setting_type == "throttle":
            field = self.ctrl_temp_throttle
            method = Command.temp_throttle_on
            label = "Temp Throttle On"
            range_text = "(1-254)"
        elif setting_type == "fan_on":
            field = self.ctrl_temp_fan_on
            method = Command.temp_fan_on
            label = "Temp Fan On"
            range_text = "(1-80)"
        elif setting_type == "fan_off":
            field = self.ctrl_temp_fan_off
            method = Command.temp_fan_off
            label = "Temp Fan Off"
            range_text = "(0-79)"
        elif setting_type == "critical":
            field = self.ctrl_temp_critical
            method = Command.temp_critical
            label = "Temp Critical"
            range_text = "(2-90)"
        else:
            return

        value = field.text().strip()
        if not value or not value.isdigit():
            self.statusBar().showMessage(f"Enter a valid temperature {range_text}")
            return

        reply = QMessageBox.question(self, "Confirm Setting",
            f"Set {label} to {value}°C?",
            QMessageBox.StandardButton.Yes | QMessageBox.StandardButton.No)

        if reply == QMessageBox.StandardButton.Yes:
            try:
                self.send_command(method(int(value)))
            except ValueError as e:
                self.statusBar().showMessage(str(e))

    def confirm_and_send_default(self):
        """Confirm and send set default command"""
        reply = QMessageBox.question(self, "Confirm Setting",
            "Save current settings as default?",
            QMessageBox.StandardButton.Yes | QMessageBox.StandardButton.No)

        if reply == QMessageBox.StandardButton.Yes:
            self.send_command(Command.SET_DEFAULT)

    def confirm_and_send_reset(self):
        """Confirm and send reset defaults command"""
        reply = QMessageBox.question(self, "Confirm Reset",
            "Reset all settings to factory defaults?\nThis cannot be undone.",
            QMessageBox.StandardButton.Yes | QMessageBox.StandardButton.No)

        if reply == QMessageBox.StandardButton.Yes:
            self.send_command(Command.RESET_DEFAULTS)

    def on_status_changed(self, status):
        """Handle status message updates"""
        self.statusBar().showMessage(status)

    def on_connection_state(self, connected):
        """Handle connection state changes - updates UI but doesn't clean up worker reference.

        The worker may emit False during reconnection but still be running.
        Only clean up the worker reference when the thread actually finishes (in on_worker_finished).
        """
        if connected:
            self.status_label.setText("● Connected")
            self.status_label.setStyleSheet("color: #4caf50; font-weight: bold; letter-spacing: 0.5px;")
            self.statusBar().clearMessage()
            self.connect_btn.setText("Disconnect")
            self.port_combo.setEnabled(False)
            self.mode_normal_btn.setEnabled(True)
            self.mode_manual_btn.setEnabled(True)
            self.command_input.setEnabled(True)
            self.send_command_btn.setEnabled(True)
            self.read_settings_btn.setEnabled(True)
            self.ctrl_pwm_a.setEnabled(True)
            self.ctrl_pwm_b.setEnabled(True)
            self.ctrl_temp_throttle.setEnabled(True)
            self.ctrl_temp_fan_on.setEnabled(True)
            self.ctrl_temp_fan_off.setEnabled(True)
            self.ctrl_temp_critical.setEnabled(True)
            self.ctrl_pwm_a_btn.setEnabled(True)
            self.ctrl_pwm_b_btn.setEnabled(True)
            self.ctrl_temp_throttle_btn.setEnabled(True)
            self.ctrl_temp_fan_on_btn.setEnabled(True)
            self.ctrl_temp_fan_off_btn.setEnabled(True)
            self.ctrl_temp_critical_btn.setEnabled(True)
            self.set_default_btn.setEnabled(True)
            self.reset_defaults_btn.setEnabled(True)
        else:
            self.status_label.setText("● Disconnected")
            self.status_label.setStyleSheet("color: #e05555; font-weight: bold; letter-spacing: 0.5px;")
            self.statusBar().showMessage("Disconnected")
            self.connect_btn.setText("Connect")
            self.connect_btn.setEnabled(True)
            self.port_combo.setEnabled(True)
            self.mode_normal_btn.setEnabled(False)
            self.mode_manual_btn.setEnabled(False)
            self.command_input.setEnabled(False)
            self.send_command_btn.setEnabled(False)
            self.read_settings_btn.setEnabled(False)
            self.ctrl_pwm_a.setEnabled(False)
            self.ctrl_pwm_b.setEnabled(False)
            self.ctrl_temp_throttle.setEnabled(False)
            self.ctrl_temp_fan_on.setEnabled(False)
            self.ctrl_temp_fan_off.setEnabled(False)
            self.ctrl_temp_critical.setEnabled(False)
            self.ctrl_pwm_a_btn.setEnabled(False)
            self.ctrl_pwm_b_btn.setEnabled(False)
            self.ctrl_temp_throttle_btn.setEnabled(False)
            self.ctrl_temp_fan_on_btn.setEnabled(False)
            self.ctrl_temp_fan_off_btn.setEnabled(False)
            self.ctrl_temp_critical_btn.setEnabled(False)
            self.set_default_btn.setEnabled(False)
            self.reset_defaults_btn.setEnabled(False)

    def clear_output(self):
        """Clear the raw data display"""
        self.raw_text.clear()

    def autoscroll_to_bottom(self):
        """Scroll text display to bottom if autoscroll enabled"""
        if self.autoscroll_enabled:
            scrollbar = self.raw_text.verticalScrollBar()
            scrollbar.setValue(scrollbar.maximum())

    def on_autoscroll_toggled(self, state):
        """Handle autoscroll checkbox toggle"""
        self.autoscroll_enabled = self.autoscroll_checkbox.isChecked()
        if self.autoscroll_enabled:
            self.autoscroll_to_bottom()

    def send_command(self, command: str):
        """Send a command to the device"""
        if not self.serial_worker or not self.serial_worker.isRunning():
            self.statusBar().showMessage("Not connected - cannot send command")
            return

        if command == Command.SETTINGS_READ:
            self.accumulated_settings.clear()

        self.raw_text.insertPlainText(f"\n>>> {command}\n")
        if self.serial_worker.ser and self.serial_worker.ser.is_open:
            self.serial_worker.ser.write(f"{command}\r\n".encode())
        self.autoscroll_to_bottom()

    def send_custom_command(self):
        """Send a custom command from the command input field"""
        command = self.command_input.text().strip()
        if not command:
            return

        self.send_command(command)
        self.command_input.clear()

    def closeEvent(self, event):
        """Handle application close event"""
        self.find_ports_timer.stop()
        if self.serial_worker:
            self.disconnect()
        event.accept()


def main():
    """Application entry point"""
    app = QApplication(sys.argv)
    app.setStyleSheet(DARK_STYLESHEET)
    window = SerialMonitorUI()
    window.show()
    sys.exit(app.exec())


if __name__ == "__main__":
    main()
