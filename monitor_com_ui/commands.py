"""Device command builders"""


class Command:
    """Unified interface for building device commands"""

    # Mode commands
    MODE_NORMAL = "MODE=NORMAL"
    MODE_MANUAL = "MODE=MANUAL"

    # Settings commands
    SETTINGS_READ = "SETTINGS?"
    RESET_DEFAULTS = "SETDEFAULT"
    SET_DEFAULT = "SETDEFAULT"
    GET_FW_VERSION = "GETFW"

    @staticmethod
    def fan(fan_num: int, state: str) -> str:
        """Build fan control command. State should be 'ON' or 'OFF'"""
        if not (1 <= fan_num <= 4):
            raise ValueError(f"Fan number must be 1-4, got {fan_num}")
        if state not in ("ON", "OFF"):
            raise ValueError(f"State must be ON or OFF, got {state}")
        return f"FAN{fan_num}={state}"

    @staticmethod
    def pwm_throttle(channel: str, value: int) -> str:
        """Build PWM throttle command. Channel should be 'A' or 'B', value 0-100"""
        if channel not in ("A", "B"):
            raise ValueError(f"Channel must be A or B, got {channel}")
        if not (0 <= value <= 100):
            raise ValueError(f"PWM value must be 0-100, got {value}")
        return f"PWMTHR={channel},{value}"

    @staticmethod
    def temp_throttle_on(temp: int) -> str:
        """Build temperature throttle ON command"""
        if not (1 <= temp <= 254):
            raise ValueError(f"Temperature must be 1-254, got {temp}")
        return f"PWMTHRTEMP={temp}"

    @staticmethod
    def temp_fan_on(temp: int) -> str:
        """Build temperature fan ON command"""
        if not (1 <= temp <= 80):
            raise ValueError(f"Temperature must be 1-80, got {temp}")
        return f"FANTEMPON={temp}"

    @staticmethod
    def temp_fan_off(temp: int) -> str:
        """Build temperature fan OFF command"""
        if not (0 <= temp <= 79):
            raise ValueError(f"Temperature must be 0-79, got {temp}")
        return f"FANTEMPOFF={temp}"

    @staticmethod
    def temp_critical(temp: int) -> str:
        """Build temperature critical command"""
        if not (2 <= temp <= 90):
            raise ValueError(f"Temperature must be 2-90, got {temp}")
        return f"TEMPCRIT={temp}"

    @staticmethod
    def set_all_settings(low_temp: int, high_temp: int, throttle_temp: int,
                        critical_temp: int, throttle_a: int, throttle_b: int) -> str:
        """Build SETTINGS command to set all parameters at once"""
        if not (0 <= low_temp <= 254):
            raise ValueError(f"Low temp must be 0-254, got {low_temp}")
        if not (0 <= high_temp <= 254):
            raise ValueError(f"High temp must be 0-254, got {high_temp}")
        if not (0 <= throttle_temp <= 254):
            raise ValueError(f"Throttle temp must be 0-254, got {throttle_temp}")
        if not (0 <= critical_temp <= 254):
            raise ValueError(f"Critical temp must be 0-254, got {critical_temp}")
        if not (0 <= throttle_a <= 100):
            raise ValueError(f"Throttle A must be 0-100, got {throttle_a}")
        if not (0 <= throttle_b <= 100):
            raise ValueError(f"Throttle B must be 0-100, got {throttle_b}")
        if low_temp >= high_temp:
            raise ValueError(f"Low temp {low_temp} must be < High temp {high_temp}")
        if high_temp >= throttle_temp:
            raise ValueError(f"High temp {high_temp} must be < Throttle temp {throttle_temp}")
        if throttle_temp >= critical_temp:
            raise ValueError(f"Throttle temp {throttle_temp} must be < Critical temp {critical_temp}")
        return f"SETTINGS={low_temp},{high_temp},{throttle_temp},{critical_temp},{throttle_a},{throttle_b}"
