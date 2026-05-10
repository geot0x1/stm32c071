import sys
import serial
import serial.tools.list_ports
import time
from PyQt6.QtWidgets import (
    QApplication, QMainWindow, QWidget, QVBoxLayout, QHBoxLayout,
    QTabWidget, QTextEdit, QPushButton, QComboBox, QLabel, QStatusBar,
    QFrame, QCheckBox, QGridLayout, QLineEdit, QScrollArea, QTableWidget, QTableWidgetItem
)
from PyQt6.QtGui import QIntValidator, QColor, QFont
from PyQt6.QtCore import QDateTime, QThread, pyqtSignal, QTimer, Qt

BAUD = 115200
TINYUSB_VID = 0xCAFE
TINYUSB_CDC_PID = 0x4000
MAX_RETRIES = 5
INITIAL_RETRY_DELAY = 0.5


class SerialWorker(QThread):
    data_received = pyqtSignal(str)
    status_changed = pyqtSignal(str)
    connection_state = pyqtSignal(bool)

    def __init__(self, port):
        super().__init__()
        self.port = port
        self.running = True
        self.ser = None

    def _connect_with_retry(self):
        retry_count = 0
        current_delay = INITIAL_RETRY_DELAY

        while retry_count < MAX_RETRIES and self.running:
            try:
                self.ser = serial.Serial(self.port, BAUD, timeout=0.1)
                self.ser.reset_input_buffer()
                self.status_changed.emit(f"Connected to {self.port}")
                self.connection_state.emit(True)
                return True
            except serial.SerialException as e:
                self._cleanup_partial_connection()
                retry_count += 1
                if retry_count < MAX_RETRIES:
                    self.status_changed.emit(
                        f"Connection failed: {e} - Retrying in {current_delay:.1f}s ({retry_count}/{MAX_RETRIES})"
                    )
                    time.sleep(current_delay)
                    current_delay = min(current_delay * 2, 5.0)
                else:
                    self.status_changed.emit(f"Failed to connect after {MAX_RETRIES} attempts: {e}")
                    self.connection_state.emit(False)
                    return False
            except Exception as e:
                self._cleanup_partial_connection()
                retry_count += 1
                if retry_count < MAX_RETRIES:
                    self.status_changed.emit(
                        f"Unexpected error: {e} - Retrying in {current_delay:.1f}s ({retry_count}/{MAX_RETRIES})"
                    )
                    time.sleep(current_delay)
                    current_delay = min(current_delay * 2, 5.0)
                else:
                    self.status_changed.emit(f"Failed after {MAX_RETRIES} attempts: {e}")
                    self.connection_state.emit(False)
                    return False

        return False

    def _cleanup_partial_connection(self):
        if self.ser:
            try:
                if self.ser.is_open:
                    self.ser.close()
            except Exception:
                pass
            self.ser = None

    def _is_port_available(self):
        try:
            available_ports = [p.device for p in serial.tools.list_ports.comports()]
            return self.port in available_ports
        except Exception:
            return False

    def run(self):
        if not self._connect_with_retry():
            return

        try:
            while self.running:
                read_errors = 0
                max_consecutive_errors = 3

                try:
                    while self.running:
                        if not self.ser or not self.ser.is_open:
                            raise serial.SerialException("Port closed")

                        if not self._is_port_available():
                            raise serial.SerialException("Port disconnected by system")

                        try:
                            line = self.ser.readline()
                            if line:
                                read_errors = 0
                                text = line.decode('utf-8', errors='ignore')
                                text = text.rstrip('\r\n') + '\n'
                                self.data_received.emit(text)
                            else:
                                time.sleep(0.01)
                        except serial.SerialException as e:
                            read_errors += 1
                            if read_errors >= max_consecutive_errors:
                                raise
                            time.sleep(0.05)
                        except UnicodeDecodeError:
                            self.data_received.emit(f"\n[DECODE ERROR]\n")
                            read_errors += 1
                            if read_errors >= max_consecutive_errors:
                                raise serial.SerialException("Multiple decode errors")
                        except Exception as e:
                            read_errors += 1
                            if read_errors >= max_consecutive_errors:
                                raise serial.SerialException(str(e))
                            time.sleep(0.05)

                except serial.SerialException as e:
                    self._cleanup_partial_connection()
                    if self.running:
                        self.connection_state.emit(False)
                        retry_count = 0
                        current_delay = INITIAL_RETRY_DELAY

                        while self.running:
                            self.status_changed.emit(
                                f"Connection lost - Retrying in {current_delay:.1f}s..."
                            )
                            time.sleep(current_delay)

                            try:
                                self.ser = serial.Serial(self.port, BAUD, timeout=0.1)
                                self.ser.reset_input_buffer()
                                self.status_changed.emit(f"Reconnected to {self.port}")
                                self.connection_state.emit(True)
                                break
                            except (serial.SerialException, Exception):
                                retry_count += 1
                                current_delay = min(current_delay * 2, 5.0)
                                if not self.running:
                                    break
                        else:
                            continue

                        if self.running:
                            continue
                        else:
                            break
                    else:
                        break

        except Exception as e:
            self.status_changed.emit(f"Thread error: {e}")
            self.connection_state.emit(False)
        finally:
            self._cleanup_partial_connection()

    def stop(self):
        self.running = False
        if self.ser:
            try:
                if self.ser.is_open:
                    self.ser.close()
            except Exception:
                pass
            self.ser = None


class SerialMonitorUI(QMainWindow):
    def __init__(self):
        super().__init__()
        self.setWindowTitle("STM32 Serial Monitor")
        self.setGeometry(100, 100, 1200, 700)

        self.serial_worker = None
        self.autoscroll_enabled = True
        self.find_ports_timer = QTimer()
        self.find_ports_timer.timeout.connect(self.update_port_list)
        self.find_ports_timer.start(1000)

        self.init_ui()
        self.update_port_list()

    def init_ui(self):
        central_widget = QWidget()
        self.setCentralWidget(central_widget)

        main_layout = QHBoxLayout()
        main_layout.setContentsMargins(0, 0, 0, 0)
        main_layout.setSpacing(0)
        left_layout = QVBoxLayout()
        left_layout.setContentsMargins(10, 8, 10, 8)
        left_layout.setSpacing(6)

        # Connection control layout
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

        # Mode control layout
        mode_layout = QHBoxLayout()
        mode_layout.addWidget(QLabel("Mode:"))

        self.mode_normal_btn = QPushButton("Normal")
        self.mode_normal_btn.clicked.connect(self.send_mode_normal)
        self.mode_normal_btn.setStyleSheet(
            "QPushButton { background-color: #1a4a8a; color: #7ec8f8; border: 1px solid #2d6abf; border-radius: 5px; font-weight: 700; padding: 5px 16px; }"
            "QPushButton:hover { background-color: #2055a0; }"
            "QPushButton:pressed { background-color: #153878; }"
            "QPushButton:disabled { background-color: #1e2128; color: #444; border-color: #252930; }"
        )
        self.mode_normal_btn.setEnabled(False)
        mode_layout.addWidget(self.mode_normal_btn)

        self.mode_manual_btn = QPushButton("Manual")
        self.mode_manual_btn.clicked.connect(self.send_mode_manual)
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

        # PWM Throttle control layout
        throttle_layout = QHBoxLayout()
        throttle_layout.addWidget(QLabel("PWM Throttle (0-100):"))

        self.throttle_a_input = QLineEdit()
        self.throttle_a_input.setPlaceholderText("Channel A")
        self.throttle_a_input.setMaxLength(3)
        self.throttle_a_input.setValidator(QIntValidator(0, 100))
        self.throttle_a_input.setEnabled(False)
        throttle_layout.addWidget(self.throttle_a_input)

        self.throttle_b_input = QLineEdit()
        self.throttle_b_input.setPlaceholderText("Channel B")
        self.throttle_b_input.setMaxLength(3)
        self.throttle_b_input.setValidator(QIntValidator(0, 100))
        self.throttle_b_input.setEnabled(False)
        throttle_layout.addWidget(self.throttle_b_input)

        self.throttle_apply_btn = QPushButton("Apply")
        self.throttle_apply_btn.setStyleSheet(
            "QPushButton { background-color: #7a4a00; color: #ffc46e; border: 1px solid #b87800; border-radius: 5px; font-weight: 700; padding: 5px 14px; }"
            "QPushButton:hover { background-color: #8a5500; }"
            "QPushButton:pressed { background-color: #5a3600; }"
            "QPushButton:disabled { background-color: #1e2128; color: #444; border-color: #252930; }"
        )
        self.throttle_apply_btn.setEnabled(False)
        self.throttle_apply_btn.clicked.connect(self.send_pwm_throttle)
        throttle_layout.addWidget(self.throttle_apply_btn)

        throttle_layout.addStretch()

        left_layout.addLayout(throttle_layout)

        # Fan control layout
        fan_container_layout = QVBoxLayout()
        fan_label = QLabel("Fans:")
        fan_label.setStyleSheet("font-weight: bold;")
        fan_container_layout.addWidget(fan_label)

        fan_layout = QGridLayout()
        fan_layout.setSpacing(8)

        for fan_num in range(1, 5):
            col = fan_num - 1

            on_btn = QPushButton(f"FAN{fan_num} ON")
            on_btn.setStyleSheet(
                "QPushButton { background-color: #1a4a22; color: #7ee89a; border: 1px solid #2d8040; border-radius: 5px; font-weight: 700; }"
                "QPushButton:hover { background-color: #205530; }"
                "QPushButton:pressed { background-color: #133318; }"
                "QPushButton:disabled { background-color: #1e2128; color: #444; border-color: #252930; }"
            )
            on_btn.setFixedSize(100, 30)
            on_btn.setEnabled(False)
            on_btn.clicked.connect(lambda checked, n=fan_num: self.send_fan_on(n))
            fan_layout.addWidget(on_btn, 0, col)
            setattr(self, f"fan{fan_num}_on_btn", on_btn)

            off_btn = QPushButton(f"FAN{fan_num} OFF")
            off_btn.setStyleSheet(
                "QPushButton { background-color: #4a1a1a; color: #f88080; border: 1px solid #802020; border-radius: 5px; font-weight: 700; }"
                "QPushButton:hover { background-color: #5a2020; }"
                "QPushButton:pressed { background-color: #3a1010; }"
                "QPushButton:disabled { background-color: #1e2128; color: #444; border-color: #252930; }"
            )
            off_btn.setFixedSize(100, 30)
            off_btn.setEnabled(False)
            off_btn.clicked.connect(lambda checked, n=fan_num: self.send_fan_off(n))
            fan_layout.addWidget(off_btn, 1, col)
            setattr(self, f"fan{fan_num}_off_btn", off_btn)

        fan_container_layout.addLayout(fan_layout)

        left_layout.addLayout(fan_container_layout)

        self.tabs = QTabWidget()

        self.raw_text = QTextEdit()
        self.raw_text.setReadOnly(True)
        font = QFont("Consolas", 9)
        self.raw_text.setFont(font)
        self.tabs.addTab(self.raw_text, "Raw Data")

        parsed_widget = QWidget()
        parsed_layout = QVBoxLayout()
        parsed_layout.setContentsMargins(0, 0, 0, 0)
        parsed_layout.setSpacing(0)

        # Telemetry table
        self.telemetry_table = QTableWidget()
        self.telemetry_table.setColumnCount(14)
        self.telemetry_table.setHorizontalHeaderLabels([
            "Received", "External Sensor", "OnBoard Sensor", "RH(%)", "State",
            "Uptime", "FAN1", "FAN2", "FAN3", "FAN4", "In A(%)", "Out A(%)", "In B(%)", "Out B(%)"
        ])
        self.telemetry_table.setAlternatingRowColors(True)
        self.telemetry_table.verticalHeader().setVisible(False)
        self.telemetry_table.horizontalHeader().setStretchLastSection(True)
        self.telemetry_table.setColumnWidth(0, 105)
        self.telemetry_table.setColumnWidth(1, 115)
        self.telemetry_table.setColumnWidth(2, 115)
        self.telemetry_table.setColumnWidth(3, 55)
        self.telemetry_table.setColumnWidth(4, 100)
        self.telemetry_table.setColumnWidth(5, 70)
        self.telemetry_table.setColumnWidth(6, 50)
        self.telemetry_table.setColumnWidth(7, 50)
        self.telemetry_table.setColumnWidth(8, 50)
        self.telemetry_table.setColumnWidth(9, 50)
        self.telemetry_table.setColumnWidth(10, 60)
        self.telemetry_table.setColumnWidth(11, 65)
        self.telemetry_table.setColumnWidth(12, 60)

        parsed_layout.addWidget(self.telemetry_table)

        parsed_widget.setLayout(parsed_layout)
        self.tabs.addTab(parsed_widget, "Telemetry")

        # Control tab
        control_widget = QWidget()
        control_layout = QVBoxLayout()

        # Read settings section
        read_section = QFrame()
        read_section.setStyleSheet("QFrame { border: 1px solid #2d4a7a; border-radius: 8px; padding: 10px; background-color: #1e2430; }")
        read_layout = QHBoxLayout()
        read_layout.addWidget(QLabel("Read Current Settings:"))
        self.read_settings_btn = QPushButton("Read Settings")
        self.read_settings_btn.setStyleSheet(
            "QPushButton { background-color: #1a3d7a; color: #7ec8f8; border: 1px solid #2d6abf; border-radius: 5px; font-weight: 700; padding: 5px 14px; }"
            "QPushButton:hover { background-color: #2055a0; }"
            "QPushButton:pressed { background-color: #122d5a; }"
            "QPushButton:disabled { background-color: #1e2128; color: #444; border-color: #252930; }"
        )
        self.read_settings_btn.setEnabled(False)
        self.read_settings_btn.clicked.connect(self.send_read_settings)
        read_layout.addWidget(self.read_settings_btn)
        read_layout.addStretch()
        read_section.setLayout(read_layout)
        control_layout.addWidget(read_section)

        # Current settings display
        display_frame = QFrame()
        display_frame.setStyleSheet("QFrame { border: 1px solid #2a2f3a; border-radius: 6px; padding: 8px; background-color: #141720; }")
        display_layout = QGridLayout()
        display_layout.setSpacing(10)

        self.settings_display = QTextEdit()
        self.settings_display.setReadOnly(True)
        self.settings_display.setFont(QFont("Consolas", 9))
        self.settings_display.setMaximumHeight(120)
        display_layout.addWidget(self.settings_display, 0, 0, 1, 2)

        display_frame.setLayout(display_layout)
        control_layout.addWidget(display_frame)

        # Settings controls
        settings_frame = QFrame()
        settings_frame.setStyleSheet("QFrame { border: 1px solid #5a3a00; border-radius: 8px; padding: 10px; background-color: #1e1e16; }")
        settings_layout = QGridLayout()
        settings_layout.setSpacing(10)

        # PWM Throttle A
        settings_layout.addWidget(QLabel("PWM Throttle A (0-100%):"), 0, 0)
        self.ctrl_pwm_a = QLineEdit()
        self.ctrl_pwm_a.setValidator(QIntValidator(0, 100))
        self.ctrl_pwm_a.setMaxLength(3)
        self.ctrl_pwm_a.setEnabled(False)
        settings_layout.addWidget(self.ctrl_pwm_a, 0, 1)
        btn_a = QPushButton("Set")
        btn_a.setEnabled(False)
        btn_a.clicked.connect(lambda: self.send_set_pwm_a())
        btn_a.setFixedWidth(60)
        settings_layout.addWidget(btn_a, 0, 2)
        self.ctrl_pwm_a_btn = btn_a

        # PWM Throttle B
        settings_layout.addWidget(QLabel("PWM Throttle B (0-100%):"), 1, 0)
        self.ctrl_pwm_b = QLineEdit()
        self.ctrl_pwm_b.setValidator(QIntValidator(0, 100))
        self.ctrl_pwm_b.setMaxLength(3)
        self.ctrl_pwm_b.setEnabled(False)
        settings_layout.addWidget(self.ctrl_pwm_b, 1, 1)
        btn_b = QPushButton("Set")
        btn_b.setEnabled(False)
        btn_b.clicked.connect(lambda: self.send_set_pwm_b())
        btn_b.setFixedWidth(60)
        settings_layout.addWidget(btn_b, 1, 2)
        self.ctrl_pwm_b_btn = btn_b

        # Temp Throttle On
        settings_layout.addWidget(QLabel("Temp Throttle On (1-254°C):"), 2, 0)
        self.ctrl_temp_throttle = QLineEdit()
        self.ctrl_temp_throttle.setValidator(QIntValidator(0, 254))
        self.ctrl_temp_throttle.setMaxLength(3)
        self.ctrl_temp_throttle.setEnabled(False)
        settings_layout.addWidget(self.ctrl_temp_throttle, 2, 1)
        btn_throttle = QPushButton("Set")
        btn_throttle.setEnabled(False)
        btn_throttle.clicked.connect(lambda: self.send_set_temp_throttle())
        btn_throttle.setFixedWidth(60)
        settings_layout.addWidget(btn_throttle, 2, 2)
        self.ctrl_temp_throttle_btn = btn_throttle

        # Temp Fan On
        settings_layout.addWidget(QLabel("Temp Fan On (1-80°C):"), 3, 0)
        self.ctrl_temp_fan_on = QLineEdit()
        self.ctrl_temp_fan_on.setValidator(QIntValidator(0, 254))
        self.ctrl_temp_fan_on.setMaxLength(3)
        self.ctrl_temp_fan_on.setEnabled(False)
        settings_layout.addWidget(self.ctrl_temp_fan_on, 3, 1)
        btn_fan_on = QPushButton("Set")
        btn_fan_on.setEnabled(False)
        btn_fan_on.clicked.connect(lambda: self.send_set_temp_fan_on())
        btn_fan_on.setFixedWidth(60)
        settings_layout.addWidget(btn_fan_on, 3, 2)
        self.ctrl_temp_fan_on_btn = btn_fan_on

        # Temp Fan Off
        settings_layout.addWidget(QLabel("Temp Fan Off (0-79°C):"), 4, 0)
        self.ctrl_temp_fan_off = QLineEdit()
        self.ctrl_temp_fan_off.setValidator(QIntValidator(0, 254))
        self.ctrl_temp_fan_off.setMaxLength(3)
        self.ctrl_temp_fan_off.setEnabled(False)
        settings_layout.addWidget(self.ctrl_temp_fan_off, 4, 1)
        btn_fan_off = QPushButton("Set")
        btn_fan_off.setEnabled(False)
        btn_fan_off.clicked.connect(lambda: self.send_set_temp_fan_off())
        btn_fan_off.setFixedWidth(60)
        settings_layout.addWidget(btn_fan_off, 4, 2)
        self.ctrl_temp_fan_off_btn = btn_fan_off

        # Temp Critical
        settings_layout.addWidget(QLabel("Temp Critical (2-90°C):"), 5, 0)
        self.ctrl_temp_critical = QLineEdit()
        self.ctrl_temp_critical.setValidator(QIntValidator(0, 254))
        self.ctrl_temp_critical.setMaxLength(3)
        self.ctrl_temp_critical.setEnabled(False)
        settings_layout.addWidget(self.ctrl_temp_critical, 5, 1)
        btn_critical = QPushButton("Set")
        btn_critical.setEnabled(False)
        btn_critical.clicked.connect(lambda: self.send_set_temp_critical())
        btn_critical.setFixedWidth(60)
        settings_layout.addWidget(btn_critical, 5, 2)
        self.ctrl_temp_critical_btn = btn_critical

        settings_frame.setLayout(settings_layout)
        control_layout.addWidget(settings_frame)

        # Reset button
        reset_layout = QHBoxLayout()
        reset_layout.addStretch()
        self.reset_defaults_btn = QPushButton("Reset to Defaults")
        self.reset_defaults_btn.setStyleSheet(
            "QPushButton { background-color: #5a1a1a; color: #f88080; border: 1px solid #902020; border-radius: 5px; font-weight: 700; padding: 5px 14px; }"
            "QPushButton:hover { background-color: #6a2020; }"
            "QPushButton:pressed { background-color: #3a1010; }"
            "QPushButton:disabled { background-color: #1e2128; color: #444; border-color: #252930; }"
        )
        self.reset_defaults_btn.setEnabled(False)
        self.reset_defaults_btn.clicked.connect(self.send_reset_to_defaults)
        reset_layout.addWidget(self.reset_defaults_btn)
        control_layout.addLayout(reset_layout)

        control_layout.addStretch()
        control_widget.setLayout(control_layout)

        left_layout.addWidget(self.tabs)

        # Command input layout (at the bottom)
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

        self.clear_input_btn = QPushButton("Clear Input")
        self.clear_input_btn.setStyleSheet(
            "QPushButton { background-color: #252930; color: #8090a0; border: 1px solid #3a3f4b; border-radius: 5px; font-weight: 600; }"
            "QPushButton:hover { background-color: #2e3440; }"
            "QPushButton:pressed { background-color: #1e2128; }"
            "QPushButton:disabled { background-color: #1e2128; color: #444; border-color: #252930; }"
        )
        self.clear_input_btn.setFixedWidth(100)
        self.clear_input_btn.setEnabled(False)
        self.clear_input_btn.clicked.connect(self.clear_command_input)
        command_layout.addWidget(self.clear_input_btn)

        left_layout.addLayout(command_layout)

        main_layout.addLayout(left_layout)

        # Right panel - Control settings
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

        # Use scroll area for control widget
        scroll_area = QScrollArea()
        scroll_area.setWidgetResizable(True)
        scroll_area.setWidget(control_widget)
        scroll_area.setStyleSheet("QScrollArea { background-color: #1a1d23; border: none; }")
        right_scroll.addWidget(scroll_area)

        right_panel.setLayout(right_scroll)
        main_layout.addWidget(right_panel)

        central_widget.setLayout(main_layout)

        self.setStatusBar(QStatusBar())
        self.statusBar().showMessage("Ready")

    def find_stm_port(self):
        for port in serial.tools.list_ports.comports():
            if port.vid == TINYUSB_VID and port.pid == TINYUSB_CDC_PID:
                return port.device
        return None

    def get_all_ports(self):
        ports = []
        for port in serial.tools.list_ports.comports():
            ports.append(port.device)
        return ports

    def update_port_list(self):
        current = self.port_combo.currentText()
        ports = self.get_all_ports()

        if self.port_combo.count() != len(ports) or (
            len(ports) > 0 and current != self.port_combo.currentText()
        ):
            self.port_combo.clear()
            self.port_combo.addItems(ports)

            stm_port = self.find_stm_port()
            if stm_port and stm_port in ports:
                self.port_combo.setCurrentText(stm_port)

    def toggle_connection(self):
        if self.serial_worker is None:
            self.connect()
        else:
            self.disconnect()

    def connect(self):
        port = self.port_combo.currentText()
        if not port:
            self.status_label.setText("● No port selected")
            self.status_label.setStyleSheet("color: #e05555; font-weight: bold; letter-spacing: 0.5px;")
            return

        self.serial_worker = SerialWorker(port)
        self.serial_worker.data_received.connect(self.on_data_received)
        self.serial_worker.status_changed.connect(self.on_status_changed)
        self.serial_worker.connection_state.connect(self.on_connection_state)
        self.serial_worker.start()

    def disconnect(self):
        if self.serial_worker:
            self.serial_worker.stop()
            if not self.serial_worker.wait(2000):
                self.serial_worker.terminate()
                self.serial_worker.wait(1000)
            self.serial_worker = None
            self.on_connection_state(False)

    def on_data_received(self, data):
        self.raw_text.insertPlainText(data)
        self.parse_telemetry(data)
        self.parse_settings_response(data)
        self.autoscroll_to_bottom()

    def parse_telemetry(self, data):
        lines = data.strip().split('\n')
        for line in lines:
            if line.startswith('$01,'):
                self.parse_01_telemetry(line)

    def parse_01_telemetry(self, line):
        try:
            parts = line.rstrip('\r\n').split(',')
            if len(parts) < 11:
                return

            boot_s = int(parts[1])
            ds_temp_str = parts[2]
            hdc_temp_str = parts[3]
            hdc_rh_str = parts[4]
            fan_str = parts[5]
            in_dc_a = int(parts[6])
            out_dc_a = int(parts[7])
            in_dc_b = int(parts[8])
            out_dc_b = int(parts[9])
            state_str = parts[10]

            now = QDateTime.currentDateTime()
            boot_time = self.format_uptime(boot_s)

            fan_states = [('ON' if (i < len(fan_str) and fan_str[i] == '1') else 'OFF') for i in range(4)]

            max_rows = 100
            if self.telemetry_table.rowCount() >= max_rows:
                self.telemetry_table.removeRow(self.telemetry_table.rowCount() - 1)

            self.telemetry_table.insertRow(0)
            row = 0

            self.telemetry_table.setItem(row, 0, QTableWidgetItem(now.toString("hh:mm:ss.zzz")))
            self.telemetry_table.setItem(row, 1, QTableWidgetItem(ds_temp_str))
            self.telemetry_table.setItem(row, 2, QTableWidgetItem(hdc_temp_str))
            self.telemetry_table.setItem(row, 3, QTableWidgetItem(hdc_rh_str))
            self.telemetry_table.setItem(row, 4, QTableWidgetItem(state_str))
            self.telemetry_table.setItem(row, 5, QTableWidgetItem(boot_time))
            self.telemetry_table.setItem(row, 6, QTableWidgetItem(fan_states[0]))
            self.telemetry_table.setItem(row, 7, QTableWidgetItem(fan_states[1]))
            self.telemetry_table.setItem(row, 8, QTableWidgetItem(fan_states[2]))
            self.telemetry_table.setItem(row, 9, QTableWidgetItem(fan_states[3]))
            self.telemetry_table.setItem(row, 10, QTableWidgetItem(str(in_dc_a)))
            self.telemetry_table.setItem(row, 11, QTableWidgetItem(str(out_dc_a)))
            self.telemetry_table.setItem(row, 12, QTableWidgetItem(str(in_dc_b)))
            self.telemetry_table.setItem(row, 13, QTableWidgetItem(str(out_dc_b)))

            self.telemetry_table.scrollToTop()

            for col in range(14):
                item = self.telemetry_table.item(row, col)
                if item:
                    item.setForeground(QColor("#c8d0e0"))
                    if col == 1 or col == 2:
                        try:
                            temp_val = int(parts[col + 1]) if parts[col + 1] != "ERR" else -1
                            if temp_val >= 50:
                                item.setForeground(QColor("#f07070"))
                            elif temp_val >= 40:
                                item.setForeground(QColor("#ffa040"))
                        except (ValueError, IndexError):
                            pass
                    elif col == 4:
                        state_color = {"TEMP_LOW": "#5a9ee8", "TEMP_HIGH": "#ffa040", "TEMP_CRIT": "#f07070", "SENSOR_LOST": "#f07070"}
                        item.setForeground(QColor(state_color.get(state_str, "#c8d0e0")))
                    elif 6 <= col <= 9:
                        fan_color = "#7ee89a" if fan_states[col - 6] == "ON" else "#f07070"
                        item.setForeground(QColor(fan_color))
        except (ValueError, IndexError):
            pass

    def format_uptime(self, seconds):
        hours = seconds // 3600
        minutes = (seconds % 3600) // 60
        secs = seconds % 60
        return f"{hours:02d}:{minutes:02d}:{secs:02d}"

    def parse_settings_response(self, data):
        lines = data.strip().split('\n')
        settings = {}

        for line in lines:
            line = line.rstrip('\r\n')
            if '=' in line:
                parts = line.split('=', 1)
                if len(parts) == 2:
                    key, value = parts
                    key = key.strip()
                    value = value.strip()

                    if key in ['PWM_THROTTLE_A', 'PWM_THROTTLE_B', 'TEMP_THROTTLE_ON',
                               'TEMP_FAN_ON', 'TEMP_FAN_OFF', 'TEMP_CRITICAL']:
                        settings[key] = value

        if settings:
            self.update_settings_display(settings)

    def update_settings_display(self, settings):
        display_text = ""
        for key, value in settings.items():
            display_text += f"{key} = {value}\n"

        self.settings_display.setPlainText(display_text)

        if 'PWM_THROTTLE_A' in settings:
            self.ctrl_pwm_a.setText(settings['PWM_THROTTLE_A'])
        if 'PWM_THROTTLE_B' in settings:
            self.ctrl_pwm_b.setText(settings['PWM_THROTTLE_B'])
        if 'TEMP_THROTTLE_ON' in settings:
            self.ctrl_temp_throttle.setText(settings['TEMP_THROTTLE_ON'])
        if 'TEMP_FAN_ON' in settings:
            self.ctrl_temp_fan_on.setText(settings['TEMP_FAN_ON'])
        if 'TEMP_FAN_OFF' in settings:
            self.ctrl_temp_fan_off.setText(settings['TEMP_FAN_OFF'])
        if 'TEMP_CRITICAL' in settings:
            self.ctrl_temp_critical.setText(settings['TEMP_CRITICAL'])

    def on_status_changed(self, status):
        self.statusBar().showMessage(status)

    def on_connection_state(self, connected):
        if connected:
            self.status_label.setText("● Connected")
            self.status_label.setStyleSheet("color: #4caf50; font-weight: bold; letter-spacing: 0.5px;")
            self.connect_btn.setText("Disconnect")
            self.port_combo.setEnabled(False)
            self.mode_normal_btn.setEnabled(True)
            self.mode_manual_btn.setEnabled(True)
            self.throttle_a_input.setEnabled(True)
            self.throttle_b_input.setEnabled(True)
            self.throttle_apply_btn.setEnabled(True)
            self.command_input.setEnabled(True)
            self.send_command_btn.setEnabled(True)
            self.clear_input_btn.setEnabled(True)
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
            self.reset_defaults_btn.setEnabled(True)
            for fan_num in range(1, 5):
                getattr(self, f"fan{fan_num}_on_btn").setEnabled(True)
                getattr(self, f"fan{fan_num}_off_btn").setEnabled(True)
        else:
            self.status_label.setText("● Disconnected")
            self.status_label.setStyleSheet("color: #e05555; font-weight: bold; letter-spacing: 0.5px;")
            self.connect_btn.setText("Connect")
            self.port_combo.setEnabled(True)
            self.mode_normal_btn.setEnabled(False)
            self.mode_manual_btn.setEnabled(False)
            self.throttle_a_input.setEnabled(False)
            self.throttle_b_input.setEnabled(False)
            self.throttle_apply_btn.setEnabled(False)
            self.command_input.setEnabled(False)
            self.send_command_btn.setEnabled(False)
            self.clear_input_btn.setEnabled(False)
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
            self.reset_defaults_btn.setEnabled(False)
            for fan_num in range(1, 5):
                getattr(self, f"fan{fan_num}_on_btn").setEnabled(False)
                getattr(self, f"fan{fan_num}_off_btn").setEnabled(False)
            self.serial_worker = None

    def clear_output(self):
        self.raw_text.clear()

    def autoscroll_to_bottom(self):
        if self.autoscroll_enabled:
            scrollbar = self.raw_text.verticalScrollBar()
            scrollbar.setValue(scrollbar.maximum())

    def on_autoscroll_toggled(self, state):
        self.autoscroll_enabled = self.autoscroll_checkbox.isChecked()
        if self.autoscroll_enabled:
            self.autoscroll_to_bottom()

    def send_mode_normal(self):
        if self.serial_worker is None or not self.serial_worker.isRunning():
            self.statusBar().showMessage("Not connected - cannot send command")
            return
        self.raw_text.insertPlainText("\n>>> MODE NORMAL\n")
        if self.serial_worker.ser and self.serial_worker.ser.is_open:
            self.serial_worker.ser.write(b"MODE=NORMAL\r\n")
        self.autoscroll_to_bottom()

    def send_mode_manual(self):
        if self.serial_worker is None or not self.serial_worker.isRunning():
            self.statusBar().showMessage("Not connected - cannot send command")
            return
        self.raw_text.insertPlainText("\n>>> MODE MANUAL\n")
        if self.serial_worker.ser and self.serial_worker.ser.is_open:
            self.serial_worker.ser.write(b"MODE=MANUAL\r\n")
        self.autoscroll_to_bottom()

    def send_fan_on(self, fan_num):
        if self.serial_worker is None or not self.serial_worker.isRunning():
            self.statusBar().showMessage("Not connected - cannot send command")
            return
        self.raw_text.insertPlainText(f"\n>>> FAN{fan_num}=ON\n")
        if self.serial_worker.ser and self.serial_worker.ser.is_open:
            self.serial_worker.ser.write(f"FAN{fan_num}=ON\r\n".encode())
        self.autoscroll_to_bottom()

    def send_fan_off(self, fan_num):
        if self.serial_worker is None or not self.serial_worker.isRunning():
            self.statusBar().showMessage("Not connected - cannot send command")
            return
        self.raw_text.insertPlainText(f"\n>>> FAN{fan_num}=OFF\n")
        if self.serial_worker.ser and self.serial_worker.ser.is_open:
            self.serial_worker.ser.write(f"FAN{fan_num}=OFF\r\n".encode())
        self.autoscroll_to_bottom()

    def send_pwm_throttle(self):
        if self.serial_worker is None or not self.serial_worker.isRunning():
            self.statusBar().showMessage("Not connected - cannot send command")
            return

        throttle_a = self.throttle_a_input.text().strip()
        throttle_b = self.throttle_b_input.text().strip()

        if not throttle_a and not throttle_b:
            self.statusBar().showMessage("Please enter at least one throttle value")
            return

        if throttle_a:
            if not throttle_a.isdigit() or int(throttle_a) > 100:
                self.statusBar().showMessage("Channel A: Enter a value between 0-100")
                return
            self.raw_text.insertPlainText(f"\n>>> PWMTHR=A,{throttle_a}\n")
            if self.serial_worker.ser and self.serial_worker.ser.is_open:
                self.serial_worker.ser.write(f"PWMTHR=A,{throttle_a}\r\n".encode())

        if throttle_b:
            if not throttle_b.isdigit() or int(throttle_b) > 100:
                self.statusBar().showMessage("Channel B: Enter a value between 0-100")
                return
            self.raw_text.insertPlainText(f"\n>>> PWMTHR=B,{throttle_b}\n")
            if self.serial_worker.ser and self.serial_worker.ser.is_open:
                self.serial_worker.ser.write(f"PWMTHR=B,{throttle_b}\r\n".encode())

        self.autoscroll_to_bottom()

    def send_custom_command(self):
        if self.serial_worker is None or not self.serial_worker.isRunning():
            self.statusBar().showMessage("Not connected - cannot send command")
            return

        command = self.command_input.text().strip()
        if not command:
            return

        self.raw_text.insertPlainText(f"\n>>> {command}\n")
        if self.serial_worker.ser and self.serial_worker.ser.is_open:
            self.serial_worker.ser.write(f"{command}\r\n".encode())

        self.autoscroll_to_bottom()

    def clear_command_input(self):
        self.command_input.clear()

    def send_read_settings(self):
        if self.serial_worker is None or not self.serial_worker.isRunning():
            self.statusBar().showMessage("Not connected - cannot send command")
            return
        self.raw_text.insertPlainText("\n>>> SETTINGS?\n")
        if self.serial_worker.ser and self.serial_worker.ser.is_open:
            self.serial_worker.ser.write(b"SETTINGS?\r\n")
        self.autoscroll_to_bottom()

    def send_set_pwm_a(self):
        if self.serial_worker is None or not self.serial_worker.isRunning():
            self.statusBar().showMessage("Not connected - cannot send command")
            return
        value = self.ctrl_pwm_a.text().strip()
        if not value or not value.isdigit() or int(value) > 100:
            self.statusBar().showMessage("PWM A: Enter a value between 0-100")
            return
        cmd = f"PWMTHR=A{value}"
        self.raw_text.insertPlainText(f"\n>>> {cmd}\n")
        if self.serial_worker.ser and self.serial_worker.ser.is_open:
            self.serial_worker.ser.write(f"{cmd}\r\n".encode())
        self.autoscroll_to_bottom()

    def send_set_pwm_b(self):
        if self.serial_worker is None or not self.serial_worker.isRunning():
            self.statusBar().showMessage("Not connected - cannot send command")
            return
        value = self.ctrl_pwm_b.text().strip()
        if not value or not value.isdigit() or int(value) > 100:
            self.statusBar().showMessage("PWM B: Enter a value between 0-100")
            return
        cmd = f"PWMTHR=B{value}"
        self.raw_text.insertPlainText(f"\n>>> {cmd}\n")
        if self.serial_worker.ser and self.serial_worker.ser.is_open:
            self.serial_worker.ser.write(f"{cmd}\r\n".encode())
        self.autoscroll_to_bottom()

    def send_set_temp_throttle(self):
        if self.serial_worker is None or not self.serial_worker.isRunning():
            self.statusBar().showMessage("Not connected - cannot send command")
            return
        value = self.ctrl_temp_throttle.text().strip()
        if not value or not self._is_valid_int(value):
            self.statusBar().showMessage("Temp Throttle: Enter a valid integer (0-254)")
            return
        cmd = f"PWMTHRTEMP={value}"
        self.raw_text.insertPlainText(f"\n>>> {cmd}\n")
        if self.serial_worker.ser and self.serial_worker.ser.is_open:
            self.serial_worker.ser.write(f"{cmd}\r\n".encode())
        self.autoscroll_to_bottom()

    def send_set_temp_fan_on(self):
        if self.serial_worker is None or not self.serial_worker.isRunning():
            self.statusBar().showMessage("Not connected - cannot send command")
            return
        value = self.ctrl_temp_fan_on.text().strip()
        if not value or not self._is_valid_int(value):
            self.statusBar().showMessage("Temp Fan On: Enter a valid integer (1-80)")
            return
        cmd = f"FANTEMPON={value}"
        self.raw_text.insertPlainText(f"\n>>> {cmd}\n")
        if self.serial_worker.ser and self.serial_worker.ser.is_open:
            self.serial_worker.ser.write(f"{cmd}\r\n".encode())
        self.autoscroll_to_bottom()

    def send_set_temp_fan_off(self):
        if self.serial_worker is None or not self.serial_worker.isRunning():
            self.statusBar().showMessage("Not connected - cannot send command")
            return
        value = self.ctrl_temp_fan_off.text().strip()
        if not value or not self._is_valid_int(value):
            self.statusBar().showMessage("Temp Fan Off: Enter a valid integer (0-79)")
            return
        cmd = f"FANTEMPOFF={value}"
        self.raw_text.insertPlainText(f"\n>>> {cmd}\n")
        if self.serial_worker.ser and self.serial_worker.ser.is_open:
            self.serial_worker.ser.write(f"{cmd}\r\n".encode())
        self.autoscroll_to_bottom()

    def send_set_temp_critical(self):
        if self.serial_worker is None or not self.serial_worker.isRunning():
            self.statusBar().showMessage("Not connected - cannot send command")
            return
        value = self.ctrl_temp_critical.text().strip()
        if not value or not self._is_valid_int(value):
            self.statusBar().showMessage("Temp Critical: Enter a valid integer (2-90)")
            return
        cmd = f"TEMPCRIT={value}"
        self.raw_text.insertPlainText(f"\n>>> {cmd}\n")
        if self.serial_worker.ser and self.serial_worker.ser.is_open:
            self.serial_worker.ser.write(f"{cmd}\r\n".encode())
        self.autoscroll_to_bottom()

    def send_reset_to_defaults(self):
        if self.serial_worker is None or not self.serial_worker.isRunning():
            self.statusBar().showMessage("Not connected - cannot send command")
            return
        self.raw_text.insertPlainText("\n>>> DEFAULT\n")
        if self.serial_worker.ser and self.serial_worker.ser.is_open:
            self.serial_worker.ser.write(b"DEFAULT\r\n")
        self.autoscroll_to_bottom()

    def _is_valid_int(self, value):
        try:
            int(value)
            return True
        except ValueError:
            return False

    def closeEvent(self, event):
        self.find_ports_timer.stop()
        if self.serial_worker:
            self.disconnect()
        event.accept()


DARK_STYLESHEET = """
    QMainWindow, QWidget {
        background-color: #1a1d23;
        color: #e0e0e0;
        font-family: 'Segoe UI', sans-serif;
        font-size: 12px;
    }
    QLabel {
        color: #b0b8c8;
        background: transparent;
    }
    QComboBox {
        background-color: #252930;
        color: #e0e0e0;
        border: 1px solid #3a3f4b;
        border-radius: 5px;
        padding: 4px 8px;
        min-height: 22px;
    }
    QComboBox:hover { border-color: #5a8dee; }
    QComboBox::drop-down { border: none; }
    QComboBox QAbstractItemView {
        background-color: #252930;
        color: #e0e0e0;
        selection-background-color: #2d5da6;
        border: 1px solid #3a3f4b;
    }
    QLineEdit {
        background-color: #252930;
        color: #e0e0e0;
        border: 1px solid #3a3f4b;
        border-radius: 5px;
        padding: 4px 8px;
        min-height: 22px;
    }
    QLineEdit:hover { border-color: #5a8dee; }
    QLineEdit:focus { border-color: #5a8dee; background-color: #2a2f3a; }
    QLineEdit:disabled { background-color: #1e2128; color: #555; border-color: #2a2f3a; }
    QLineEdit::placeholder { color: #555; }
    QCheckBox {
        color: #b0b8c8;
        spacing: 6px;
    }
    QCheckBox::indicator {
        width: 14px; height: 14px;
        border: 1px solid #3a3f4b;
        border-radius: 3px;
        background-color: #252930;
    }
    QCheckBox::indicator:checked {
        background-color: #5a8dee;
        border-color: #5a8dee;
    }
    QPushButton {
        background-color: #2d3240;
        color: #c8d0e0;
        border: 1px solid #3a3f4b;
        border-radius: 5px;
        padding: 5px 14px;
        font-weight: 600;
        min-height: 24px;
    }
    QPushButton:hover { background-color: #363c4e; border-color: #5a6a88; }
    QPushButton:pressed { background-color: #252a38; }
    QPushButton:disabled { background-color: #1e2128; color: #444; border-color: #252930; }
    QTextEdit {
        background-color: #141720;
        color: #c8ffc8;
        border: 1px solid #2a2f3a;
        border-radius: 5px;
        font-family: 'Consolas', monospace;
        font-size: 11px;
        selection-background-color: #2d5da6;
    }
    QTabWidget::pane {
        border: 1px solid #2a2f3a;
        border-radius: 5px;
        background-color: #1a1d23;
    }
    QTabBar::tab {
        background-color: #1e2128;
        color: #7a8a9a;
        border: 1px solid #2a2f3a;
        border-bottom: none;
        padding: 6px 18px;
        border-top-left-radius: 5px;
        border-top-right-radius: 5px;
        margin-right: 2px;
    }
    QTabBar::tab:selected {
        background-color: #252930;
        color: #e0e0e0;
        border-color: #3a3f4b;
    }
    QTabBar::tab:hover:!selected { background-color: #22262e; color: #b0b8c8; }
    QTableWidget {
        background-color: #1a1d23;
        color: #c8d0e0;
        gridline-color: #252930;
        border: 1px solid #2a2f3a;
        border-radius: 4px;
        font-family: 'Consolas', monospace;
        font-size: 11px;
    }
    QHeaderView::section {
        background-color: #1e2128;
        color: #7a8a9a;
        border: none;
        border-right: 1px solid #2a2f3a;
        border-bottom: 1px solid #2a2f3a;
        padding: 5px 6px;
        font-family: 'Segoe UI', sans-serif;
        font-size: 11px;
        font-weight: 600;
        text-transform: uppercase;
    }
    QTableWidget::item { padding: 4px 6px; border: none; }
    QTableWidget::item:selected { background-color: #2d3a56; color: #e0e0e0; }
    QTableWidget::item:alternate { background-color: #1e2128; }
    QScrollBar:vertical {
        background: #1a1d23;
        width: 10px;
        border-radius: 5px;
    }
    QScrollBar::handle:vertical {
        background: #3a3f4b;
        border-radius: 5px;
        min-height: 20px;
    }
    QScrollBar::handle:vertical:hover { background: #5a6a88; }
    QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0px; }
    QScrollBar:horizontal {
        background: #1a1d23;
        height: 10px;
        border-radius: 5px;
    }
    QScrollBar::handle:horizontal {
        background: #3a3f4b;
        border-radius: 5px;
    }
    QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal { width: 0px; }
    QScrollArea { border: none; background: transparent; }
    QFrame { border: none; }
    QStatusBar {
        background-color: #141720;
        color: #7a8a9a;
        border-top: 1px solid #252930;
        font-size: 11px;
    }
"""


if __name__ == "__main__":
    app = QApplication(sys.argv)
    app.setStyleSheet(DARK_STYLESHEET)
    window = SerialMonitorUI()
    window.show()
    sys.exit(app.exec())
