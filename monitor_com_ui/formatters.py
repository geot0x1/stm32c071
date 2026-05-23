"""Data formatting and styling utilities"""

try:
    from .config import TemperatureThresholds
    from .models import DeviceState
except ImportError:
    from config import TemperatureThresholds
    from models import DeviceState


class TelemetryColors:
    """Color scheme for telemetry display"""
    TEMP_CRITICAL = "#f07070"
    TEMP_WARNING = "#ffa040"
    FAN_ON = "#7ee89a"
    FAN_OFF = "#f07070"
    STATE_LOW = "#5a9ee8"
    STATE_HIGH = "#ffa040"
    STATE_CRIT = "#f07070"
    STATE_LOST = "#f07070"
    DEFAULT = "#c8d0e0"


class TelemetryFormatter:
    """Format telemetry data for display with appropriate colors"""

    STATE_COLORS = {
        DeviceState.BOOT.value: TelemetryColors.DEFAULT,
        DeviceState.FAULT.value: TelemetryColors.STATE_LOST,
        DeviceState.TEMP_LOW.value: TelemetryColors.STATE_LOW,
        DeviceState.TEMP_HIGH.value: TelemetryColors.STATE_HIGH,
        DeviceState.TEMP_CRIT.value: TelemetryColors.STATE_CRIT,
        DeviceState.SENSOR_LOST.value: TelemetryColors.STATE_LOST,
    }

    @staticmethod
    def get_temp_color(temp_str: str) -> str:
        """Get color for temperature value"""
        if temp_str == "ERR":
            return TelemetryColors.TEMP_CRITICAL

        try:
            temp = int(temp_str)
            if temp >= TemperatureThresholds.CRITICAL:
                return TelemetryColors.TEMP_CRITICAL
            elif temp >= TemperatureThresholds.WARNING:
                return TelemetryColors.TEMP_WARNING
        except ValueError:
            pass

        return TelemetryColors.DEFAULT

    @staticmethod
    def get_state_color(state_str: str) -> str:
        """Get color for device state"""
        return TelemetryFormatter.STATE_COLORS.get(state_str, TelemetryColors.DEFAULT)

    @staticmethod
    def get_fan_color(is_on: bool) -> str:
        """Get color for fan state"""
        return TelemetryColors.FAN_ON if is_on else TelemetryColors.FAN_OFF
