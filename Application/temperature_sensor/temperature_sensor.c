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
static int16_t _last_temperature = INT16_MIN;
static uint8_t _fail_count = 0;

static bool _is_lost = false;

static void handle_failure(void)
{
    _fail_count++;
    if (_fail_count >= MAX_FAILURES && !_is_lost)
    {
        _is_lost = true;
        _last_temperature = INT16_MIN;
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
                _last_temperature = (int16_t)(celsius * 100.0f);
                _fail_count = 0;
                _is_lost = false;
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

int16_t get_temperature(void)
{
    return _last_temperature;
}
