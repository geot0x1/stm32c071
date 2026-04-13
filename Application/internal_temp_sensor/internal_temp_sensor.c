#include "internal_temp_sensor.h"
#include "adc.h"
#include "stm32c0xx_hal.h"

/* ── Factory calibration constants ───────────────────────────────────────────
 * STM32C071 internal temperature sensor calibration values stored in Flash.
 * Source: STM32C0 reference manual (RM0434), Section 16.3.7
 */
#define TS_CAL1_ADDR    ((const uint16_t *)0x1FFF75A8UL)   /* 30°C calibration */
#define TS_CAL2_ADDR    ((const uint16_t *)0x1FFF75CAUL)   /* 110°C calibration */
#define TS_CAL1_TEMP    30
#define TS_CAL2_TEMP    110

/* ── Configuration ───────────────────────────────────────────────────────────
 */
#define POLL_INTERVAL_MS  2000U

/* ── State machine states ────────────────────────────────────────────────────
 */
typedef enum
{
    StateConvert,
    StateWaitNext
} InternalTempState;

/* ── Module state (singleton pattern) ────────────────────────────────────────
 */
static uint16_t             _last_temp    = 0xFFFF;
static uint32_t             _next_tick    = 0;
static InternalTempHandler  _handler      = NULL;
static InternalTempState    _state        = StateConvert;

/* ── Private helpers ─────────────────────────────────────────────────────────
 */

/**
 * @brief Convert ADC raw value to Celsius * 100 using factory calibration.
 *
 * Uses linear interpolation between two factory calibration points:
 * - TS_CAL1: ADC value at 30°C
 * - TS_CAL2: ADC value at 110°C
 *
 * All math uses int32_t to prevent overflow.
 *
 * @param adc_raw 12-bit raw ADC value from TEMPSENSOR channel
 * @return Temperature in Celsius * 100 (e.g., 2500 = 25.00°C)
 */
static uint16_t adc_to_celsius_100(uint32_t adc_raw)
{
    int32_t cal1 = (int32_t)(*TS_CAL1_ADDR);
    int32_t cal2 = (int32_t)(*TS_CAL2_ADDR);
    int32_t raw  = (int32_t)adc_raw;

    /* Linear interpolation: temp_100 = (cal2 - cal1) / (temp2 - temp1) * (raw - cal1) + temp1 * 100
     * Rearranged: ((TS_CAL2_TEMP - TS_CAL1_TEMP) * 100 * (raw - cal1)) / (cal2 - cal1) + (TS_CAL1_TEMP * 100)
     */
    int32_t temp_100 = ((TS_CAL2_TEMP - TS_CAL1_TEMP) * 100 * (raw - cal1)) / (cal2 - cal1)
                       + (TS_CAL1_TEMP * 100);

    return (uint16_t)temp_100;
}

/* ── Public API ──────────────────────────────────────────────────────────────
 */

void internal_temp_sensor_init(void)
{
    _last_temp = 0xFFFF;
    _next_tick = HAL_GetTick();  /* Trigger read immediately on first task call */
    _handler = NULL;
    _state = StateConvert;
}

void internal_temp_sensor_task(void)
{
    uint32_t now = HAL_GetTick();

    switch (_state)
    {
        case StateConvert:
        {
            /* Perform blocking ADC read */
            uint32_t raw;
            if (adc_read_channel(ADC_CHANNEL_TEMPSENSOR, &raw))
            {
                _last_temp = adc_to_celsius_100(raw);
                if (_handler)
                {
                    _handler(InternalTempSensorOk);
                }
            }
            else
            {
                _last_temp = 0xFFFF;
                if (_handler)
                {
                    _handler(InternalTempSensorError);
                }
            }

            /* Schedule next read */
            _next_tick = now + POLL_INTERVAL_MS;
            _state = StateWaitNext;
            break;
        }

        case StateWaitNext:
        {
            if (now >= _next_tick)
            {
                _state = StateConvert;
            }
            break;
        }

        default:
            _state = StateConvert;
            break;
    }
}

uint16_t internal_temp_sensor_get(void)
{
    return _last_temp;
}

void internal_temp_sensor_register_handler(InternalTempHandler handler)
{
    _handler = handler;
}
