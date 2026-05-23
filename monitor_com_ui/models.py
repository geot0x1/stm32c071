"""Data models and enums"""

from enum import Enum
from dataclasses import dataclass


class DeviceState(Enum):
    """Device operating state"""
    BOOT = "BOOT"
    FAULT = "FAULT"
    TEMP_LOW = "TEMP_LOW"
    TEMP_HIGH = "TEMP_HIGH"
    TEMP_THROTTLE = "TEMP_THROTTLE"
    TEMP_CRIT = "TEMP_CRIT"
    SENSOR_LOST = "SENSOR_LOST"
    ERROR = "ERROR"


@dataclass
class TelemetryData:
    """Parsed telemetry data from device"""
    timestamp: str
    ds_temp: str
    hdc_temp: str
    hdc_rh: str
    state: DeviceState
    uptime: str
    fans: list
    pwm_in_a: int
    pwm_out_a: int
    pwm_in_b: int
    pwm_out_b: int
    button: str


@dataclass
class DeviceSettings:
    """Device configuration settings"""
    pwm_throttle_a: int
    pwm_throttle_b: int
    temp_throttle_on: int
    temp_fan_on: int
    temp_fan_off: int
    temp_critical: int


@dataclass
class SettingDefinition:
    """Definition of a configurable device setting"""
    key: str
    label: str
    min_value: int
    max_value: int
    unit: str = "°C"


DEVICE_SETTINGS_DEFS = {
    "PWM_THROTTLE_A": SettingDefinition("PWM_THROTTLE_A", "PWM Throttle A", 0, 100, "%"),
    "PWM_THROTTLE_B": SettingDefinition("PWM_THROTTLE_B", "PWM Throttle B", 0, 100, "%"),
    "TEMP_THROTTLE_ON": SettingDefinition("TEMP_THROTTLE_ON", "Temp Throttle On", 1, 254),
    "TEMP_FAN_ON": SettingDefinition("TEMP_FAN_ON", "Temp Fan On", 1, 80),
    "TEMP_FAN_OFF": SettingDefinition("TEMP_FAN_OFF", "Temp Fan Off", 0, 79),
    "TEMP_CRITICAL": SettingDefinition("TEMP_CRITICAL", "Temp Critical", 2, 90),
}
