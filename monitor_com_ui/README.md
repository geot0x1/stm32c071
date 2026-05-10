# STM32 Serial Monitor UI

A modular, clean PyQt6-based serial monitor application for STM32 microcontrollers.

## Project Structure

The application is organized into simple, focused modules for maintainability and testability:

```
monitor_com_ui/
├── main.py                 # Application entry point and main window
├── config.py              # Configuration constants (all settings in one place)
├── logger.py              # Logging setup
├── models.py              # Data classes and enums
├── parsers.py             # Message parsing logic
├── formatters.py          # Display formatting and colors
├── commands.py            # Device command builders
├── serial_worker.py       # Background serial communication thread
├── styles.py              # Dark theme stylesheet
├── telemetry_table.py     # Telemetry table display management
├── __init__.py            # Package exports
├── run.py                 # Quick launch script
├── README.md              # This file
└── REFACTORING.md         # Details on refactoring improvements
```

## Key Features

### Modular Architecture
- **Separation of Concerns**: Data parsing, formatting, and UI are separate
- **Testable**: Pure data classes and functions with no PyQt dependencies
- **Reusable**: Each module can be imported and used independently

### Clean Code Patterns
- **Data Models**: `TelemetryData`, `DeviceSettings` for type safety
- **Command Builder**: `Command` class centralizes all device commands
- **Validators**: `SettingsValidator` for configuration validation
- **Formatters**: Color and display logic separated from UI

### Thread-Safe
- `SerialWorker` runs in background thread
- PyQt signals for thread-safe communication
- Proper cleanup on disconnect with `MAX_RECONNECT_RETRIES`

### Improved Error Handling
- Logging throughout for debugging
- Validation of all inputs
- Graceful reconnection with exponential backoff

## Running the Application

```bash
# From the project root
python -m monitor_com_ui.main

# Or directly
cd monitor_com_ui
python main.py
```

## Configuration

Edit `config.py` to adjust:
- Serial baud rate, timeouts, retry counts
- UI settings (window size, telemetry table rows)
- Temperature thresholds for coloring
- Device VID/PID for auto-detection

## Adding New Features

### Adding a New Command
```python
# In commands.py
@staticmethod
def my_command(param: int) -> str:
    """Build my command"""
    return f"CMD={param}"

# Use it
self.send_command(Command.my_command(42))
```

### Adding a New Telemetry Field
```python
# 1. Update TelemetryData in models.py
@dataclass
class TelemetryData:
    new_field: str

# 2. Update TelemetryParser.parse() in parsers.py
new_field=parts[11],

# 3. Update table view in telemetry_table.py
self.table.setItem(row, 14, QTableWidgetItem(data.new_field))
```

### Adding New Color Logic
```python
# In formatters.py
@staticmethod
def get_my_color(value: str) -> str:
    if condition:
        return TelemetryColors.SOME_COLOR
    return TelemetryColors.DEFAULT

# Use it
color = self.formatter.get_my_color(value)
```

## Testing

Each module can be tested independently without UI dependencies:

```python
from monitor_com_ui import TelemetryParser, Command, SettingsValidator

# Test parsing
telemetry = TelemetryParser.parse("$01,3600,25,24,45,0011,50,45,60,55,TEMP_LOW")
assert telemetry.ds_temp == "25"

# Test commands
cmd = Command.pwm_throttle("A", 75)
assert cmd == "PWMTHR=A,75"

# Test validation
is_valid, error = SettingsValidator.validate("PWM_THROTTLE_A", 50)
assert is_valid == True
```

## Benefits of This Structure

| Before | After |
|--------|-------|
| 1200+ lines in one file | ~200 lines per module |
| Hardcoded strings scattered | Named constants in config |
| Mixed parsing and UI | Separated concerns |
| Difficult to test | Testable data classes |
| Silent failures | Logged errors |
| Duplicated send logic | Single `send_command()` method |
