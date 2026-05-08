import sys
import serial
import serial.tools.list_ports
from PyQt6.QtWidgets import (
    QApplication, QMainWindow, QWidget, QVBoxLayout, QHBoxLayout,
    QTabWidget, QTextEdit, QPushButton, QComboBox, QLabel, QStatusBar,
    QFrame
)
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
            self.status_changed.emit(f"Connected to {self.port}")
            self.connection_state.emit(True)

            while self.running:
                if self.ser and self.ser.is_open:
                    if self.ser.in_waiting > 0:
                        data = self.ser.read(self.ser.in_waiting)
                        try:
                            text = data.decode('utf-8', errors='ignore')
                            self.data_received.emit(text)
                        except Exception as e:
                            self.data_received.emit(f"\n[DECODE ERROR] {e}\n")
                    self.msleep(10)
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
        self.mode_normal_btn.setStyleSheet("background-color: #e8f4f8;")
        self.mode_normal_btn.setEnabled(False)
        mode_layout.addWidget(self.mode_normal_btn)

        self.mode_manual_btn = QPushButton("Manual")
        self.mode_manual_btn.clicked.connect(self.send_mode_manual)
        self.mode_manual_btn.setStyleSheet("background-color: #f8e8e8;")
        self.mode_manual_btn.setEnabled(False)
        mode_layout.addWidget(self.mode_manual_btn)

        mode_layout.addStretch()

        main_layout.addLayout(mode_layout)

        # Fan control layout
        fan_layout = QHBoxLayout()
        fan_layout.addWidget(QLabel("Fans:"))

        for fan_num in range(1, 5):
            on_btn = QPushButton(f"FAN{fan_num} ON")
            on_btn.setStyleSheet("background-color: #c8e6c9;")
            on_btn.setEnabled(False)
            on_btn.clicked.connect(lambda checked, n=fan_num: self.send_fan_on(n))
            fan_layout.addWidget(on_btn)
            setattr(self, f"fan{fan_num}_on_btn", on_btn)

            off_btn = QPushButton(f"FAN{fan_num} OFF")
            off_btn.setStyleSheet("background-color: #ffcccc;")
            off_btn.setEnabled(False)
            off_btn.clicked.connect(lambda checked, n=fan_num: self.send_fan_off(n))
            fan_layout.addWidget(off_btn)
            setattr(self, f"fan{fan_num}_off_btn", off_btn)

        fan_layout.addStretch()

        main_layout.addLayout(fan_layout)

        self.tabs = QTabWidget()

        self.raw_text = QTextEdit()
        self.raw_text.setReadOnly(True)
        font = QFont("Consolas", 9)
        self.raw_text.setFont(font)
        self.tabs.addTab(self.raw_text, "Raw Data")

        self.parsed_text = QTextEdit()
        self.parsed_text.setReadOnly(True)
        self.parsed_text.setFont(font)
        self.parsed_text.setText("[Parsed data view - future implementation]\n")
        self.tabs.addTab(self.parsed_text, "Parsed Data")

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
        scrollbar = self.raw_text.verticalScrollBar()
        scrollbar.setValue(scrollbar.maximum())

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
            for fan_num in range(1, 5):
                getattr(self, f"fan{fan_num}_on_btn").setEnabled(False)
                getattr(self, f"fan{fan_num}_off_btn").setEnabled(False)
            self.serial_worker = None

    def clear_output(self):
        self.raw_text.clear()

    def send_mode_normal(self):
        if self.serial_worker is None or not self.serial_worker.isRunning():
            self.statusBar().showMessage("Not connected - cannot send command")
            return
        self.raw_text.insertPlainText("\n>>> MODE NORMAL\n")
        if self.serial_worker.ser and self.serial_worker.ser.is_open:
            self.serial_worker.ser.write(b"MODE=NORMAL\r\n")
        scrollbar = self.raw_text.verticalScrollBar()
        scrollbar.setValue(scrollbar.maximum())

    def send_mode_manual(self):
        if self.serial_worker is None or not self.serial_worker.isRunning():
            self.statusBar().showMessage("Not connected - cannot send command")
            return
        self.raw_text.insertPlainText("\n>>> MODE MANUAL\n")
        if self.serial_worker.ser and self.serial_worker.ser.is_open:
            self.serial_worker.ser.write(b"MODE=MANUAL\r\n")
        scrollbar = self.raw_text.verticalScrollBar()
        scrollbar.setValue(scrollbar.maximum())

    def send_fan_on(self, fan_num):
        if self.serial_worker is None or not self.serial_worker.isRunning():
            self.statusBar().showMessage("Not connected - cannot send command")
            return
        self.raw_text.insertPlainText(f"\n>>> FAN{fan_num}=ON\n")
        if self.serial_worker.ser and self.serial_worker.ser.is_open:
            self.serial_worker.ser.write(f"FAN{fan_num}=ON\r\n".encode())
        scrollbar = self.raw_text.verticalScrollBar()
        scrollbar.setValue(scrollbar.maximum())

    def send_fan_off(self, fan_num):
        if self.serial_worker is None or not self.serial_worker.isRunning():
            self.statusBar().showMessage("Not connected - cannot send command")
            return
        self.raw_text.insertPlainText(f"\n>>> FAN{fan_num}=OFF\n")
        if self.serial_worker.ser and self.serial_worker.ser.is_open:
            self.serial_worker.ser.write(f"FAN{fan_num}=OFF\r\n".encode())
        scrollbar = self.raw_text.verticalScrollBar()
        scrollbar.setValue(scrollbar.maximum())

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
