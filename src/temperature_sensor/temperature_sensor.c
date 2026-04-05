#include "temperature_sensor.h"
#include "ds18b20.h"
#include "main.h"
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
                _failCount++;
                if (_failCount >= MAX_FAILURES)
                {
                    _lastTemperature = 0xFFFF;
                }
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
                _failCount++;
                if (_failCount >= MAX_FAILURES)
                {
                    _lastTemperature = 0xFFFF;
                }
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
            }
            else
            {
                _failCount++;
                if (_failCount >= MAX_FAILURES)
                {
                    _lastTemperature = 0xFFFF;
                }
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
