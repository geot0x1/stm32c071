"""Application configuration constants"""


class SerialConfig:
    """Serial port configuration"""
    BAUD_RATE = 115200
    TIMEOUT = 0.1
    MAX_RETRIES = 5
    MAX_RECONNECT_RETRIES = 30
    INITIAL_RETRY_DELAY = 0.5
    MAX_RETRY_DELAY = 5.0


class UIConfig:
    """UI configuration"""
    TELEMETRY_MAX_ROWS = 100
    PORT_SCAN_INTERVAL = 1000  # ms
    WINDOW_WIDTH = 1200
    WINDOW_HEIGHT = 700


class DeviceConfig:
    """Device identifiers"""
    TINYUSB_VID = 0xCAFE
    TINYUSB_CDC_PID = 0x4000


class TemperatureThresholds:
    """Temperature threshold values for coloring"""
    CRITICAL = 50
    WARNING = 40
