#include "temperature_sensor.h"
#include "ds18b20.h"
#include "stm32c0xx_hal.h"
#include <stdio.h>

typedef enum
{
    StatePoll,
    StateConvert,
    StateWaitConversion,
    StateRead,
    StateWaitNext
} TempSensorState;

#define MAX_FAILURES 3
#define TICK_1S 1000

static OneWire _ow_bus = {.port = GPIOB, .pin = GPIO_PIN_4};

static Ds18b20_t _ds18b20_dev = {.ow = &_ow_bus};

static TempSensorState _current_state = StatePoll;
static uint32_t _last_tick = 0;
static uint16_t _last_temperature = 0xFFFF;
static uint8_t _fail_count = 0;

static uint16_t _setpoint_a = 0;
static uint16_t _setpoint_b = 0;
static uint16_t _hysteresis = 0;
static TempSensorHandler _event_handler = NULL;

static bool _is_above_a = false;
static bool _is_above_b = false;
static bool _is_lost = false;

static void trigger_event(TempSensorEvent event)
{
    if (_event_handler != NULL)
    {
        _event_handler(event);
    }
}

static void handle_setpoints(uint16_t current_temp)
{
    // Setpoint A
    if (!_is_above_a && current_temp > _setpoint_a)
    {
        _is_above_a = true;
        trigger_event(AboveA);
    }
    else if (_is_above_a && (current_temp < (_setpoint_a - _hysteresis)))
    {
        _is_above_a = false;
        trigger_event(BelowA);
    }

    // Setpoint B
    if (!_is_above_b && current_temp > _setpoint_b)
    {
        _is_above_b = true;
        trigger_event(AboveB);
    }
    else if (_is_above_b && (current_temp < (_setpoint_b - _hysteresis)))
    {
        _is_above_b = false;
        trigger_event(BelowB);
    }
}

static void handle_failure(void)
{
    _fail_count++;
    if (_fail_count >= MAX_FAILURES && !_is_lost)
    {
        _is_lost = true;
        _last_temperature = 0xFFFF;
        trigger_event(SensorLost);
    }
}

void temperature_sensor_init(void)
{
    ds18b20_begin(&_ds18b20_dev);
    _current_state = StatePoll;
    _last_tick = HAL_GetTick();
}

void temperature_sensor_task(void)
{
    switch (_current_state)
    {
        case StatePoll:
            ds18b20_begin(&_ds18b20_dev); // Poll for sensors on the bus
            if (ds18b20_get_sensors_on_bus(&_ds18b20_dev) > 0)
            {
                _current_state = StateConvert;
            }
            else
            {
                handle_failure();
                _current_state = StateWaitNext;
                _last_tick = HAL_GetTick();
            }
            break;

        case StateConvert:
            if (ds18b20_request_temperatures(&_ds18b20_dev) == 0)
            {
                _current_state = StateWaitConversion;
                _last_tick = HAL_GetTick();
            }
            else
            {
                handle_failure();
                _current_state = StateWaitNext;
                _last_tick = HAL_GetTick();
            }
            break;

        case StateWaitConversion:
            if (HAL_GetTick() - _last_tick >= TICK_1S)
            {
                _current_state = StateRead;
            }
            break;

        case StateRead:
        {
            int16_t raw_temp = ds18b20_get_temp(&_ds18b20_dev, NULL);

            if (raw_temp != -127 && raw_temp != 850)
            {
                float celsius = ds18b20_raw_to_celsius(raw_temp);
                // Store as Celsius * 100; intentional: negatives wrap via int16_t (two's
                // complement)
                _last_temperature = (uint16_t)((int16_t)(celsius * 100.0f));
                _fail_count = 0;
                _is_lost = false;
                handle_setpoints(_last_temperature);
            }
            else
            {
                handle_failure();
            }
            _current_state = StateWaitNext;
            _last_tick = HAL_GetTick();
            break;
        }

        case StateWaitNext:
            if (HAL_GetTick() - _last_tick >= TICK_1S)
            {
                _current_state = StatePoll;
            }
            break;

        default:
            _current_state = StatePoll;
            break;
    }
}

uint16_t get_temperature(void)
{
    return _last_temperature;
}

void temperature_sensor_set_setpoint_a(uint16_t setpoint)
{
    _setpoint_a = setpoint;
}

void temperature_sensor_set_setpoint_b(uint16_t setpoint)
{
    _setpoint_b = setpoint;
}

void temperature_sensor_set_hysteresis(uint16_t hysteresis)
{
    _hysteresis = hysteresis;
}

void temperature_sensor_register_handler(TempSensorHandler handler)
{
    _event_handler = handler;
}
