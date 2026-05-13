"""Data parsers for serial messages"""

import logging
from typing import Optional
from PyQt6.QtCore import QDateTime

try:
    from .models import TelemetryData, DeviceState, DEVICE_SETTINGS_DEFS
except ImportError:
    from models import TelemetryData, DeviceState, DEVICE_SETTINGS_DEFS

logger = logging.getLogger(__name__)


class TelemetryParser:
    """Parser for device telemetry messages (format $01,...)"""

    EXPECTED_FIELD_COUNT = 12
    MESSAGE_PREFIX = "$01,"

    @staticmethod
    def parse(line: str) -> Optional[TelemetryData]:
        """Parse a telemetry line and return TelemetryData or None if invalid"""
        try:
            parts = line.rstrip('\r\n').split(',')
            if len(parts) < TelemetryParser.EXPECTED_FIELD_COUNT:
                return None

            boot_s = int(parts[1])
            state_str = parts[10]
            button_str = parts[11]

            try:
                state = DeviceState(state_str)
            except ValueError:
                state = DeviceState.TEMP_LOW
                logger.warning(f"Unknown device state: {state_str}")

            return TelemetryData(
                timestamp=QDateTime.currentDateTime().toString("hh:mm:ss.zzz"),
                ds_temp=parts[2],
                hdc_temp=parts[3],
                hdc_rh=parts[4],
                state=state,
                uptime=TelemetryParser.format_uptime(boot_s),
                fans=TelemetryParser.parse_fans(parts[5]),
                pwm_in_a=int(parts[6]),
                pwm_out_a=int(parts[7]),
                pwm_in_b=int(parts[8]),
                pwm_out_b=int(parts[9]),
                button=button_str,
            )
        except (ValueError, IndexError) as e:
            logger.error(f"Failed to parse telemetry '{line}': {e}")
            return None

    @staticmethod
    def format_uptime(seconds: int) -> str:
        """Format seconds as HH:MM:SS"""
        hours = seconds // 3600
        minutes = (seconds % 3600) // 60
        secs = seconds % 60
        return f"{hours:02d}:{minutes:02d}:{secs:02d}"

    @staticmethod
    def parse_fans(fan_str: str) -> list:
        """Parse fan status string into list of ON/OFF strings"""
        return ['ON' if i < len(fan_str) and fan_str[i] == '1' else 'OFF' for i in range(4)]


class SettingsParser:
    """Parser for device settings responses (format KEY=VALUE)"""

    @staticmethod
    def parse_line(line: str) -> tuple[Optional[str], Optional[str]]:
        """Parse a KEY=VALUE line and return (key, value) or (None, None)"""
        if '=' not in line:
            return None, None

        parts = line.split('=', 1)
        if len(parts) != 2:
            return None, None

        key = parts[0].strip()
        value = parts[1].strip()

        if key in DEVICE_SETTINGS_DEFS:
            return key, value

        return None, None

    @staticmethod
    def parse_settings_from_text(data: str) -> dict:
        """Extract all settings from text data"""
        settings = {}
        lines = data.strip().split('\n')

        for line in lines:
            line = line.rstrip('\r\n')
            key, value = SettingsParser.parse_line(line)
            if key and value:
                settings[key] = value

        return settings


class SettingsValidator:
    """Validator for device settings"""

    @staticmethod
    def validate(key: str, value: int) -> tuple[bool, str]:
        """Validate a setting value. Returns (is_valid, error_message)"""
        if key not in DEVICE_SETTINGS_DEFS:
            return False, f"Unknown setting: {key}"

        setting = DEVICE_SETTINGS_DEFS[key]
        if not (setting.min_value <= value <= setting.max_value):
            return False, f"{setting.label} must be {setting.min_value}-{setting.max_value} {setting.unit}"

        return True, ""
