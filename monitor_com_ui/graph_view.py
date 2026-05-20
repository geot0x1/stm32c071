"""Real-time graph visualization for telemetry data"""

from collections import deque
from PyQt6.QtWidgets import QWidget
from PyQt6.QtGui import QPainter, QPen, QColor, QFont
from PyQt6.QtCore import Qt

try:
    from .models import TelemetryData, DeviceState
except ImportError:
    from models import TelemetryData, DeviceState


class GraphView(QWidget):
    """Real-time graph display for temperature and state data"""

    MAX_POINTS = 200

    STATE_COLORS = {
        DeviceState.TEMP_LOW: 0,
        DeviceState.TEMP_HIGH: 1,
        DeviceState.TEMP_THROTTLE: 2,
        DeviceState.TEMP_CRIT: 3,
        DeviceState.SENSOR_LOST: 4,
        DeviceState.ERROR: 5,
    }

    STATE_NAMES = {
        0: "Low",
        1: "High",
        2: "Throttle",
        3: "Critical",
        4: "Sensor Lost",
        5: "Error",
    }

    def __init__(self):
        super().__init__()
        self.times = deque(maxlen=self.MAX_POINTS)
        self.ds_temps = deque(maxlen=self.MAX_POINTS)
        self.hdc_temps = deque(maxlen=self.MAX_POINTS)
        self.states = deque(maxlen=self.MAX_POINTS)
        self.sample_count = 0

        self.setStyleSheet("background-color: #1a1d23;")
        self.temp_min = 0
        self.temp_max = 150

    def add_telemetry(self, telemetry: TelemetryData):
        """Add a new telemetry data point"""
        ds_temp = None
        hdc_temp = None

        try:
            ds_temp = float(telemetry.ds_temp)
        except (ValueError, TypeError, AttributeError):
            pass

        try:
            hdc_temp = float(telemetry.hdc_temp)
        except (ValueError, TypeError, AttributeError):
            pass

        self.times.append(self.sample_count)
        self.ds_temps.append(ds_temp)
        self.hdc_temps.append(hdc_temp)
        self.states.append(self.STATE_COLORS.get(telemetry.state, 0))
        self.sample_count += 1

        self.update()

    def paintEvent(self, event):
        """Draw the graph"""
        painter = QPainter(self)
        painter.setRenderHint(QPainter.RenderHint.Antialiasing)

        width = self.width()
        height = self.height()

        margin_left = 60
        margin_right = 80
        margin_top = 30
        margin_bottom = 40

        plot_width = width - margin_left - margin_right
        plot_height = height - margin_top - margin_bottom

        painter.fillRect(0, 0, width, height, QColor("#1a1d23"))

        painter.setPen(QPen(QColor("#252930"), 1))
        painter.drawRect(margin_left, margin_top, plot_width, plot_height)

        painter.setPen(QPen(QColor("#7ec8f8"), 1))
        font = QFont("Consolas", 9)
        painter.setFont(font)

        painter.drawText(10, height - 10, "Temperature (°C)")
        painter.save()
        painter.translate(20, height // 2)
        painter.rotate(-90)
        painter.drawText(0, 0, "Temperature (°C)")
        painter.restore()

        painter.setPen(QPen(QColor("#7ef87e"), 1))
        painter.drawText(width - 70, height - 10, "State")
        painter.save()
        painter.translate(width - 15, height // 2)
        painter.rotate(-90)
        painter.drawText(0, 0, "State")
        painter.restore()

        if len(self.times) < 1:
            painter.setPen(QPen(QColor("#7a8a9a"), 1))
            painter.setFont(QFont("Consolas", 11))
            painter.drawText(width // 2 - 150, height // 2, "Waiting for telemetry data...")
            return

        times_list = list(self.times)
        ds_temps_list = list(self.ds_temps)
        hdc_temps_list = list(self.hdc_temps)
        states_list = list(self.states)

        time_min = times_list[0]
        time_max = times_list[-1]
        time_range = time_max - time_min if time_max > time_min else 1

        temp_min = 0
        temp_max = 150
        temp_range = temp_max - temp_min

        def map_x(t):
            return margin_left + (t - time_min) / time_range * plot_width

        def map_y_temp(temp):
            if temp is None:
                return None
            return margin_top + (temp_max - temp) / temp_range * plot_height

        def map_y_state(state):
            return margin_top + (5 - state) / 5.0 * plot_height

        painter.setPen(QPen(QColor("#ff6b6b"), 2))
        for i in range(len(ds_temps_list) - 1):
            if ds_temps_list[i] is not None and ds_temps_list[i + 1] is not None:
                y1 = map_y_temp(ds_temps_list[i])
                y2 = map_y_temp(ds_temps_list[i + 1])
                if y1 is not None and y2 is not None:
                    x1 = map_x(times_list[i])
                    x2 = map_x(times_list[i + 1])
                    painter.drawLine(int(x1), int(y1), int(x2), int(y2))

        painter.setPen(QPen(QColor("#4ecdc4"), 2))
        for i in range(len(hdc_temps_list) - 1):
            if hdc_temps_list[i] is not None and hdc_temps_list[i + 1] is not None:
                y1 = map_y_temp(hdc_temps_list[i])
                y2 = map_y_temp(hdc_temps_list[i + 1])
                if y1 is not None and y2 is not None:
                    x1 = map_x(times_list[i])
                    x2 = map_x(times_list[i + 1])
                    painter.drawLine(int(x1), int(y1), int(x2), int(y2))

        painter.setPen(QPen(QColor("#ffd93d"), 2))
        for i in range(len(states_list) - 1):
            y1 = map_y_state(states_list[i])
            y2 = map_y_state(states_list[i + 1])
            x1 = map_x(times_list[i])
            x2 = map_x(times_list[i + 1])
            painter.drawLine(int(x1), int(y1), int(x2), int(y2))

        painter.setPen(QPen(QColor("#7a8a9a"), 1))
        painter.setFont(QFont("Consolas", 8))

        for i in range(6):
            y = margin_top + i / 5.0 * plot_height
            painter.drawText(width - 75, int(y), self.STATE_NAMES.get(5 - i, ""))

        painter.setPen(QPen(QColor("#7ec8f8"), 1))
        painter.drawText(margin_left - 50, margin_top - 10, "DS18B20")

        painter.setPen(QPen(QColor("#4ecdc4"), 1))
        painter.drawText(margin_left - 50, margin_top, "HDC2010")

        painter.setPen(QPen(QColor("#7ec8f8"), 1))
        painter.setFont(QFont("Consolas", 7))
        for i in range(5):
            y = margin_top + i / 4.0 * plot_height
            temp_val = temp_max - (temp_max - temp_min) * i / 4.0
            painter.drawText(margin_left - 55, int(y), f"{temp_val:.0f}")
        painter.drawText(margin_left - 55, margin_top + plot_height, f"{temp_min:.0f}")

        if len(self.ds_temps) > 0:
            last_ds = self.ds_temps[-1]
            last_hdc = self.hdc_temps[-1]
            painter.setPen(QPen(QColor("#ff6b6b"), 1))
            painter.setFont(QFont("Consolas", 9))
            painter.drawText(margin_left + 10, margin_top + 20, f"DS18B20: {last_ds if last_ds else 'N/A'}")
            painter.setPen(QPen(QColor("#4ecdc4"), 1))
            painter.drawText(margin_left + 10, margin_top + 35, f"HDC2010: {last_hdc if last_hdc else 'N/A'}")
