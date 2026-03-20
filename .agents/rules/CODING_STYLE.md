---
trigger: always_on
---

## C Style Guide

### Naming
- Functions: `snake_case` → `read_sensor()`
- Types (struct/enum/typedef): `PascalCase` → `SensorConfig`
- Global variables: `lowerCamelCase` → `systemTick`
- Static/local: `snake_case` → `is_ready`
- Constants/macros: `ALL_CAPS` → `MAX_ADC`

### Formatting
- Braces required for all control statements
- Allman style (braces on new line)
- Indent: 4 spaces, no tabs

### Example
```c
#define LED_DELAY_MS 500

typedef enum
{
    StateIdle,
    StateActive,
    StateError
} DeviceState;

uint32_t statusInterval = 1000;

void toggle_status(void)
{
    static bool _led_on = false;
    uint8_t _error = 0;

    if (statusInterval > 0)
    {
        _led_on = !_led_on;
        HAL_GPIO_TogglePin(GPIOA, GPIO_PIN_5);
    }
    else
    {
        _error = 1;
    }
}