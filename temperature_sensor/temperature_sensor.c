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

#define MAX_FAILURES    3
#define TICK_1S         1000

static OneWire _owBus = {
    .port = GPIOB,
    .pin = GPIO_PIN_4
};

static Ds18b20_t _ds18b20 = {
    .ow = &_owBus
};

static TempSensorState _currentState = StatePoll;
static uint32_t _lastTick = 0;
static uint16_t _lastTemperature = 0xFFFF;
static uint8_t _failCount = 0;

static uint16_t _setpoint_a = 0;
static uint16_t _setpoint_b = 0;
static uint16_t _hysteresis = 0;
static TempSensorHandler _eventHandler = NULL;

static bool _isAboveA = false;
static bool _isAboveB = false;
static bool _isLost = false;

static void trigger_event(TempSensorEvent event)
{
    if (_eventHandler != NULL)
    {
        _eventHandler(event);
    }
}

static void handle_setpoints(uint16_t current_temp)
{
    // Setpoint A
    if (!_isAboveA && current_temp > _setpoint_a)
    {
        _isAboveA = true;
        trigger_event(EVENT_ABOVE_A);
    }
    else if (_isAboveA && (current_temp < (_setpoint_a - _hysteresis)))
    {
        _isAboveA = false;
        trigger_event(EVENT_BELOW_A);
    }

    // Setpoint B
    if (!_isAboveB && current_temp > _setpoint_b)
    {
        _isAboveB = true;
        trigger_event(EVENT_ABOVE_B);
    }
    else if (_isAboveB && (current_temp < (_setpoint_b - _hysteresis)))
    {
        _isAboveB = false;
        trigger_event(EVENT_BELOW_B);
    }
}

static void handle_failure(void)
{
    _failCount++;
    if (_failCount >= MAX_FAILURES && !_isLost)
    {
        _isLost = true;
        _lastTemperature = 0xFFFF;
        trigger_event(EVENT_SENSOR_LOST);
    }
}

void temperature_sensor_init(void)
{
    ds18b20_begin(&_ds18b20);
    _currentState = StatePoll;
    _lastTick = HAL_GetTick();
}

void temperature_sensor_tick(void)
{
    switch (_currentState)
    {
        case StatePoll:
            ds18b20_begin(&_ds18b20); // Poll for sensors on the bus
            if (ds18b20_get_sensors_on_bus(&_ds18b20) > 0)
            {
                _currentState = StateConvert;
            }
            else
            {
                handle_failure();
                _currentState = StateWaitNext;
                _lastTick = HAL_GetTick();
            }
            break;

        case StateConvert:
            if (ds18b20_request_temperatures(&_ds18b20) == 0)
            {
                _currentState = StateWaitConversion;
                _lastTick = HAL_GetTick();
            }
            else
            {
                handle_failure();
                _currentState = StateWaitNext;
                _lastTick = HAL_GetTick();
            }
            break;

        case StateWaitConversion:
            if (HAL_GetTick() - _lastTick >= TICK_1S)
            {
                _currentState = StateRead;
            }
            break;

        case StateRead:
        {
            int16_t raw_temp = ds18b20_get_temp(&_ds18b20, NULL);
            
            if (raw_temp != -127 && raw_temp != 850)
            {
                float celsius = ds18b20_raw_to_celsius(raw_temp);
                // Store as Celsius * 100
                _lastTemperature = (uint16_t)((int16_t)(celsius * 100.0f));
                _failCount = 0;
                _isLost = false;
                handle_setpoints(_lastTemperature);
            }
            else
            {
                handle_failure();
            }
            _currentState = StateWaitNext;
            _lastTick = HAL_GetTick();
            break;
        }

        case StateWaitNext:
            if (HAL_GetTick() - _lastTick >= TICK_1S)
            {
                _currentState = StatePoll;
            }
            break;

        default:
            _currentState = StatePoll;
            break;
    }
}

uint16_t get_temperature(void)
{
    return _lastTemperature;
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
    _eventHandler = handler;
}
