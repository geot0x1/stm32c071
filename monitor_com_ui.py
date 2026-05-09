import sys
import serial
import serial.tools.list_ports
from PyQt6.QtWidgets import (
    QApplication, QMainWindow, QWidget, QVBoxLayout, QHBoxLayout,
    QTabWidget, QTextEdit, QPushButton, QComboBox, QLabel, QStatusBar,
    QFrame, QCheckBox, QGridLayout, QLineEdit
)
from PyQt6.QtGui import QIntValidator
from PyQt6.QtCore import QDateTime
from PyQt6.QtCore import QThread, pyqtSignal, QTimer, Qt
from PyQt6.QtGui import QColor, QFont

BAUD = 115200
TINYUSB_VID = 0xCAFE
TINYUSB_CDC_PID = 0x4000


class SerialWorker(QThread):
    data_received = pyqtSignal(str)
    status_changed = pyqtSignal(str)
    connection_state = pyqtSignal(bool)

    def __init__(self, port):
        super().__init__()
        self.port = port
        self.running = True
        self.ser = None

    def run(self):
        try:
            self.ser = serial.Serial(self.port, BAUD, timeout=0.1)
            self.ser.reset_input_buffer()
            self.status_changed.emit(f"Connected to {self.port}")
            self.connection_state.emit(True)

            while self.running:
                if self.ser and self.ser.is_open:
                    try:
                        line = self.ser.readline()
                        if line:
                            text = line.decode('utf-8', errors='ignore')
                            text = text.rstrip('\r\n') + '\n'
                            self.data_received.emit(text)
                    except Exception as e:
                        self.data_received.emit(f"\n[DECODE ERROR] {e}\n")
                else:
                    break

        except serial.SerialException as e:
            self.status_changed.emit(f"Serial Error: {e}")
            self.connection_state.emit(False)
        except Exception as e:
            self.status_changed.emit(f"Error: {e}")
            self.connection_state.emit(False)
        finally:
            if self.ser and self.ser.is_open:
                self.ser.close()
            self.connection_state.emit(False)

    def stop(self):
        self.running = False
        if self.ser and self.ser.is_open:
            self.ser.close()


class SerialMonitorUI(QMainWindow):
    def __init__(self):
        super().__init__()
        self.setWindowTitle("STM32 Serial Monitor")
        self.setGeometry(100, 100, 1000, 600)

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

        main_layout = QVBoxLayout()

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

        self.status_label = QLabel("Disconnected")
        self.status_label.setStyleSheet("color: red; font-weight: bold;")
        control_layout.addWidget(self.status_label)

        main_layout.addLayout(control_layout)

        # Mode control layout
        mode_layout = QHBoxLayout()
        mode_layout.addWidget(QLabel("Mode:"))

        self.mode_normal_btn = QPushButton("Normal")
        self.mode_normal_btn.clicked.connect(self.send_mode_normal)
        self.mode_normal_btn.setStyleSheet("background-color: #2196F3; color: white; font-weight: bold;")
        self.mode_normal_btn.setEnabled(False)
        mode_layout.addWidget(self.mode_normal_btn)

        self.mode_manual_btn = QPushButton("Manual")
        self.mode_manual_btn.clicked.connect(self.send_mode_manual)
        self.mode_manual_btn.setStyleSheet("background-color: #FF6B6B; color: white; font-weight: bold;")
        self.mode_manual_btn.setEnabled(False)
        mode_layout.addWidget(self.mode_manual_btn)

        mode_layout.addStretch()

        main_layout.addLayout(mode_layout)

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
        self.throttle_apply_btn.setStyleSheet("background-color: #FF9800; color: white; font-weight: bold;")
        self.throttle_apply_btn.setEnabled(False)
        self.throttle_apply_btn.clicked.connect(self.send_pwm_throttle)
        throttle_layout.addWidget(self.throttle_apply_btn)

        throttle_layout.addStretch()

        main_layout.addLayout(throttle_layout)

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
            on_btn.setStyleSheet("background-color: #4CAF50; color: white; font-weight: bold;")
            on_btn.setFixedSize(100, 32)
            on_btn.setEnabled(False)
            on_btn.clicked.connect(lambda checked, n=fan_num: self.send_fan_on(n))
            fan_layout.addWidget(on_btn, 0, col)
            setattr(self, f"fan{fan_num}_on_btn", on_btn)

            off_btn = QPushButton(f"FAN{fan_num} OFF")
            off_btn.setStyleSheet("background-color: #FF5252; color: white; font-weight: bold;")
            off_btn.setFixedSize(100, 32)
            off_btn.setEnabled(False)
            off_btn.clicked.connect(lambda checked, n=fan_num: self.send_fan_off(n))
            fan_layout.addWidget(off_btn, 1, col)
            setattr(self, f"fan{fan_num}_off_btn", off_btn)

        fan_container_layout.addLayout(fan_layout)

        main_layout.addLayout(fan_container_layout)

        self.tabs = QTabWidget()

        self.raw_text = QTextEdit()
        self.raw_text.setReadOnly(True)
        font = QFont("Consolas", 9)
        self.raw_text.setFont(font)
        self.tabs.addTab(self.raw_text, "Raw Data")

        parsed_widget = QWidget()
        parsed_layout = QVBoxLayout()

        # Current data display section
        current_frame = QFrame()
        current_frame.setStyleSheet("border: 2px solid #2196F3; border-radius: 12px; padding: 15px; background-color: #1a1a1a;")
        current_layout = QGridLayout()
        current_layout.setSpacing(12)

        self.datetime_label = QLabel("Waiting for data...")
        self.datetime_label.setStyleSheet("font-weight: bold; color: #2196F3; font-size: 14px;")
        current_layout.addWidget(self.datetime_label, 0, 0, 1, 4)

        card_style = "background-color: #2a2a2a; border: 1px solid #444; border-radius: 6px; padding: 8px; margin: 2px;"

        # Temperature
        self.temp_title = QLabel("Temperature")
        self.temp_title.setStyleSheet("font-size: 11px; color: #999; font-weight: bold;")
        self.temp_value = QLabel("--")
        self.temp_value.setStyleSheet(f"{card_style} font-size: 20px; font-weight: bold; color: #FF6B6B; text-align: center;")
        current_layout.addWidget(self.temp_title, 1, 0)
        current_layout.addWidget(self.temp_value, 2, 0)

        # Thermal State
        self.state_title = QLabel("State")
        self.state_title.setStyleSheet("font-size: 11px; color: #999; font-weight: bold;")
        self.state_value = QLabel("--")
        self.state_value.setStyleSheet(f"{card_style} font-size: 16px; font-weight: bold; color: #FFA500; text-align: center;")
        current_layout.addWidget(self.state_title, 1, 1)
        current_layout.addWidget(self.state_value, 2, 1)

        # Uptime
        self.uptime_title = QLabel("Uptime")
        self.uptime_title.setStyleSheet("font-size: 11px; color: #999; font-weight: bold;")
        self.uptime_value = QLabel("--")
        self.uptime_value.setStyleSheet(f"{card_style} font-size: 16px; color: #66BB6A; font-weight: bold; text-align: center;")
        current_layout.addWidget(self.uptime_title, 1, 2)
        current_layout.addWidget(self.uptime_value, 2, 2)

        # Fan states
        self.fans_title = QLabel("Fans")
        self.fans_title.setStyleSheet("font-size: 11px; color: #999; font-weight: bold;")
        self.fans_value = QLabel("--")
        self.fans_value.setStyleSheet(f"{card_style} font-size: 13px; font-weight: bold; color: #FFF; text-align: center;")
        current_layout.addWidget(self.fans_title, 1, 3)
        current_layout.addWidget(self.fans_value, 2, 3)

        # PWM Input A
        self.input_a_title = QLabel("Input A")
        self.input_a_title.setStyleSheet("font-size: 11px; color: #999; font-weight: bold;")
        self.input_a_value = QLabel("--")
        self.input_a_value.setStyleSheet(f"{card_style} font-size: 18px; color: #2196F3; font-weight: bold; text-align: center;")
        current_layout.addWidget(self.input_a_title, 3, 0)
        current_layout.addWidget(self.input_a_value, 4, 0)

        # PWM Output A
        self.output_a_title = QLabel("Output A")
        self.output_a_title.setStyleSheet("font-size: 11px; color: #999; font-weight: bold;")
        self.output_a_value = QLabel("--")
        self.output_a_value.setStyleSheet(f"{card_style} font-size: 18px; color: #4CAF50; font-weight: bold; text-align: center;")
        current_layout.addWidget(self.output_a_title, 3, 1)
        current_layout.addWidget(self.output_a_value, 4, 1)

        # PWM Input B
        self.input_b_title = QLabel("Input B")
        self.input_b_title.setStyleSheet("font-size: 11px; color: #999; font-weight: bold;")
        self.input_b_value = QLabel("--")
        self.input_b_value.setStyleSheet(f"{card_style} font-size: 18px; color: #2196F3; font-weight: bold; text-align: center;")
        current_layout.addWidget(self.input_b_title, 3, 2)
        current_layout.addWidget(self.input_b_value, 4, 2)

        # PWM Output B
        self.output_b_title = QLabel("Output B")
        self.output_b_title.setStyleSheet("font-size: 11px; color: #999; font-weight: bold;")
        self.output_b_value = QLabel("--")
        self.output_b_value.setStyleSheet(f"{card_style} font-size: 18px; color: #4CAF50; font-weight: bold; text-align: center;")
        current_layout.addWidget(self.output_b_title, 3, 3)
        current_layout.addWidget(self.output_b_value, 4, 3)

        current_frame.setLayout(current_layout)
        parsed_layout.addWidget(current_frame)

        # History section
        history_label = QLabel("History:")
        history_label.setStyleSheet("font-weight: bold; margin-top: 15px;")
        parsed_layout.addWidget(history_label)

        self.parsed_history = QTextEdit()
        self.parsed_history.setReadOnly(True)
        self.parsed_history.setFont(font)
        parsed_layout.addWidget(self.parsed_history)

        parsed_widget.setLayout(parsed_layout)
        self.tabs.addTab(parsed_widget, "Parsed Data")

        main_layout.addWidget(self.tabs)

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
            self.status_label.setText("No port selected")
            self.status_label.setStyleSheet("color: red; font-weight: bold;")
            return

        self.serial_worker = SerialWorker(port)
        self.serial_worker.data_received.connect(self.on_data_received)
        self.serial_worker.status_changed.connect(self.on_status_changed)
        self.serial_worker.connection_state.connect(self.on_connection_state)
        self.serial_worker.start()

    def disconnect(self):
        if self.serial_worker:
            self.serial_worker.stop()
            self.serial_worker.wait()
            self.serial_worker = None

    def on_data_received(self, data):
        self.raw_text.insertPlainText(data)
        self.parse_telemetry(data)
        self.autoscroll_to_bottom()

    def parse_telemetry(self, data):
        lines = data.strip().split('\n')
        for line in lines:
            if line.startswith('$01,'):
                self.parse_01_telemetry(line)

    def parse_01_telemetry(self, line):
        try:
            parts = line.rstrip('\r\n').split(',')
            if len(parts) < 9:
                return

            boot_s = int(parts[1])
            temp_str = parts[2]
            fan_str = parts[3]
            in_dc_a = int(parts[4])
            out_dc_a = int(parts[5])
            in_dc_b = int(parts[6])
            out_dc_b = int(parts[7])
            state_str = parts[8]

            # Update current display
            now = QDateTime.currentDateTime()
            self.datetime_label.setText(f"Last received: {now.toString('yyyy-MM-dd hh:mm:ss.zzz')}")

            boot_time = self.format_uptime(boot_s)
            self.uptime_value.setText(boot_time)

            if temp_str == "ERR":
                self.temp_value.setText("SENSOR ERROR")
                self.temp_value.setStyleSheet("font-size: 14px; font-weight: bold; color: #FF0000;")
            else:
                temp_int = int(temp_str)
                self.temp_value.setText(f"{temp_int}°C")
                if temp_int < 30:
                    self.temp_value.setStyleSheet("font-size: 14px; font-weight: bold; color: #2196F3;")
                elif temp_int < 50:
                    self.temp_value.setStyleSheet("font-size: 14px; font-weight: bold; color: #FFA500;")
                else:
                    self.temp_value.setStyleSheet("font-size: 14px; font-weight: bold; color: #FF0000;")

            self.state_value.setText(state_str)
            state_color = {"TEMP_LOW": "#2196F3", "TEMP_HIGH": "#FFA500", "TEMP_CRIT": "#FF0000", "SENSOR_LOST": "#FF0000"}
            self.state_value.setStyleSheet(f"font-size: 14px; font-weight: bold; color: {state_color.get(state_str, '#666')};")

            fan_display_list = []
            for i, char in enumerate(fan_str):
                fan_status = f"FAN{i+1}:{'ON' if char == '1' else 'OFF'}"
                fan_display_list.append(fan_status)

            fan_text = "  |  ".join(fan_display_list)
            self.fans_value.setText(fan_text)
            self.fans_value.setStyleSheet("font-size: 12px; font-weight: bold; color: #333;")

            self.input_a_value.setText(f"{in_dc_a}%")
            self.output_a_value.setText(f"{out_dc_a}%")
            self.input_b_value.setText(f"{in_dc_b}%")
            self.output_b_value.setText(f"{out_dc_b}%")

            # Add to history
            history_line = (
                f"[{now.toString('hh:mm:ss.zzz')}] Uptime: {boot_time} | "
                f"Temp: {self.temp_value.text()} | State: {state_str} | "
                f"Fans: {' | '.join(fan_display_list)} | "
                f"A:{in_dc_a}%→{out_dc_a}% B:{in_dc_b}%→{out_dc_b}%\n"
            )
            self.parsed_history.insertPlainText(history_line)
        except (ValueError, IndexError):
            pass

    def format_uptime(self, seconds):
        hours = seconds // 3600
        minutes = (seconds % 3600) // 60
        secs = seconds % 60
        return f"{hours:02d}:{minutes:02d}:{secs:02d}"

    def on_status_changed(self, status):
        self.statusBar().showMessage(status)

    def on_connection_state(self, connected):
        if connected:
            self.status_label.setText("Connected")
            self.status_label.setStyleSheet("color: green; font-weight: bold;")
            self.connect_btn.setText("Disconnect")
            self.port_combo.setEnabled(False)
            self.mode_normal_btn.setEnabled(True)
            self.mode_manual_btn.setEnabled(True)
            self.throttle_a_input.setEnabled(True)
            self.throttle_b_input.setEnabled(True)
            self.throttle_apply_btn.setEnabled(True)
            for fan_num in range(1, 5):
                getattr(self, f"fan{fan_num}_on_btn").setEnabled(True)
                getattr(self, f"fan{fan_num}_off_btn").setEnabled(True)
        else:
            self.status_label.setText("Disconnected")
            self.status_label.setStyleSheet("color: red; font-weight: bold;")
            self.connect_btn.setText("Connect")
            self.port_combo.setEnabled(True)
            self.mode_normal_btn.setEnabled(False)
            self.mode_manual_btn.setEnabled(False)
            self.throttle_a_input.setEnabled(False)
            self.throttle_b_input.setEnabled(False)
            self.throttle_apply_btn.setEnabled(False)
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
            scrollbar = self.parsed_history.verticalScrollBar()
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

    def closeEvent(self, event):
        self.find_ports_timer.stop()
        if self.serial_worker:
            self.disconnect()
        event.accept()


if __name__ == "__main__":
    app = QApplication(sys.argv)
    window = SerialMonitorUI()
    window.show()
    sys.exit(app.exec())
