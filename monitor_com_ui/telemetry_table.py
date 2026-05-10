"""Telemetry table display component"""

from PyQt6.QtWidgets import QTableWidget, QTableWidgetItem
from PyQt6.QtGui import QColor

try:
    from .config import UIConfig
    from .formatters import TelemetryFormatter
    from .models import TelemetryData
except ImportError:
    from config import UIConfig
    from formatters import TelemetryFormatter
    from models import TelemetryData


class TelemetryTableView:
    """Manages telemetry data display in a table widget"""

    def __init__(self, table_widget: QTableWidget):
        self.table = table_widget
        self.formatter = TelemetryFormatter()

    def add_row(self, data: TelemetryData):
        """Add a telemetry row to the table"""
        if self.table.rowCount() >= UIConfig.TELEMETRY_MAX_ROWS:
            self.table.setRowCount(0)

        self.table.insertRow(0)
        self._populate_row(0, data)
        self._style_row(0, data)
        self.table.scrollToTop()

    def _populate_row(self, row: int, data: TelemetryData):
        """Populate row cells with telemetry data"""
        self.table.setItem(row, 0, QTableWidgetItem(data.timestamp))
        self.table.setItem(row, 1, QTableWidgetItem(data.ds_temp))
        self.table.setItem(row, 2, QTableWidgetItem(data.hdc_temp))
        self.table.setItem(row, 3, QTableWidgetItem(data.hdc_rh))
        self.table.setItem(row, 4, QTableWidgetItem(data.state.value))
        self.table.setItem(row, 5, QTableWidgetItem(data.uptime))
        self.table.setItem(row, 6, QTableWidgetItem(data.fans[0]))
        self.table.setItem(row, 7, QTableWidgetItem(data.fans[1]))
        self.table.setItem(row, 8, QTableWidgetItem(data.fans[2]))
        self.table.setItem(row, 9, QTableWidgetItem(data.fans[3]))
        self.table.setItem(row, 10, QTableWidgetItem(str(data.pwm_in_a)))
        self.table.setItem(row, 11, QTableWidgetItem(str(data.pwm_out_a)))
        self.table.setItem(row, 12, QTableWidgetItem(str(data.pwm_in_b)))
        self.table.setItem(row, 13, QTableWidgetItem(str(data.pwm_out_b)))

    def _style_row(self, row: int, data: TelemetryData):
        """Apply color styling to row based on data values"""
        for col in range(14):
            item = self.table.item(row, col)
            if not item:
                continue

            color = self._get_cell_color(col, data)
            item.setForeground(QColor(color))

    def _get_cell_color(self, col: int, data: TelemetryData) -> str:
        """Determine color for a cell based on column and data"""
        # Temperature columns
        if col == 1:
            return self.formatter.get_temp_color(data.ds_temp)
        elif col == 2:
            return self.formatter.get_temp_color(data.hdc_temp)
        # State column
        elif col == 4:
            return self.formatter.get_state_color(data.state.value)
        # Fan columns
        elif 6 <= col <= 9:
            is_on = data.fans[col - 6] == "ON"
            return self.formatter.get_fan_color(is_on)
        # Default color
        else:
            return "#c8d0e0"
