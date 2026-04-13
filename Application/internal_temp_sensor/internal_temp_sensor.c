#include "internal_temp_sensor.h"
#include "adc.h"
#include "stm32c0xx_hal.h"

/* ── Factory calibration constants ───────────────────────────────────────────
 * STM32C071 internal temperature sensor calibration values stored in Flash.
 * Source: STM32C0 reference manual (RM0490), Section 16.3.6, page 321
 *
 * STM32C0 has ONE factory calibration point at 30°C stored at 0x1FFF7568.
 * Temperature is calculated using the formula from RM0490, page 321:
 *   Temperature (°C) = (TS_DATA - TS_CAL1) / Avg_Slope_Code + T_CAL1
 *
 * The STM32C071 has a POSITIVE temperature coefficient (+2.5 mV/°C typical).
 * As the chip heats up, the sensor voltage (and ADC count) increases.
 * This differs from older STM32F1 which have negative coefficients.
 *
 * Critical: Calibration (TS_CAL1) was performed at Vdda = 3.0V, but the board
 * runs at 3.3V. The ADC raw count scales with Vdda. The raw reading must be
 * normalized to the 3.0V reference using VREFINT before applying the formula.
 */
#define TS_CAL1_ADDR        ((const uint16_t *)0x1FFF7568UL) /* TS_CAL1: raw ADC @ 30°C, Vdda=3.0V */
#define TS_CAL1_TEMP        30
#define TS_AVG_SLOPE_UV     (2500)   /* Avg_Slope: +2.5 mV/°C (positive: ADC increases as temp rises) */
#define TS_VREF_CAL_MV      3000     /* Calibration Vdda reference: 3.0V */
#define TS_VDDA_MV          3300     /* Actual board supply voltage: 3.3V */
#define TS_AVGSLOPE_CODE    ((TS_AVG_SLOPE_UV * 4096) / TS_VREF_CAL_MV)  /* = +3413 (counts/°C) */

/* ── Configuration ───────────────────────────────────────────────────────────
 */
#define POLL_INTERVAL_MS 2000U

/* ── State machine states ────────────────────────────────────────────────────
 */
typedef enum
{
    StateConvert,
    StateWaitNext
} InternalTempState;

/* ── Module state (singleton pattern) ────────────────────────────────────────
 */
static int16_t _last_temp = -1;  /* -1 represents invalid/not-ready */
static uint32_t _last_raw = 0xFFFF;
static uint32_t _next_tick = 0;
static InternalTempHandler _handler = NULL;
static InternalTempState _state = StateConvert;

/* ── Private helpers ─────────────────────────────────────────────────────────
 */

/**
 * @brief Convert ADC raw value to Celsius * 100 using factory calibration.
 *
 * Uses the single-point calibration method from STM32C0 reference manual (RM0490, page 321).
 *
 * Procedure:
 *   1. Normalize ts_raw to 3.0V calibration reference:
 *      corrected = ts_raw × (TS_VDDA_MV / TS_VREF_CAL_MV)
 *   2. Apply single-point calibration formula:
 *      T × 100 = (corrected - TS_CAL1) × 100000 / TS_AVGSLOPE_CODE + 30 × 100
 *
 * The Vdda correction is essential: factory calibration was at 3.0V, but the board
 * runs at 3.3V. Without correction, the raw count is scaled differently, causing
 * systematic temperature error (~15-20°C when uncorrected).
 *
 * Max intermediate: 4095 × 3300 / 3000 = 4505; Δ = 3465 × 100000 = 346.5M < INT32_MAX ✓
 *
 * @param ts_raw TEMPSENSOR raw ADC value (12-bit, 0-4095)
 * @return Temperature in Celsius * 100 (e.g., 2730 = 27.30°C)
 */
static int16_t adc_to_celsius_100(uint32_t ts_raw)
{
    /* Step 1: Normalize ts_raw to 3.0V calibration reference.
     * Raw counts scale with Vdda: corrected = raw × (Vdda / Vcal)
     */
    int32_t corrected_ts = ((int32_t)ts_raw * (int32_t)TS_VDDA_MV) / (int32_t)TS_VREF_CAL_MV;

    /* Step 2: Single-point calibration formula (RM0490, Section 16.3.6).
     * T (°C) = (corrected_ts − TS_CAL1) / Avg_Slope_Code + T_CAL1
     * To get Celsius * 100 with integer arithmetic:
     *   T × 100 = (corrected_ts − TS_CAL1) × 100000 / TS_AVGSLOPE_CODE + 30 × 100
     */
    int32_t cal1 = (int32_t)(*TS_CAL1_ADDR);
    int32_t temp_100 = ((corrected_ts - cal1) * 100000L) / (int32_t)TS_AVGSLOPE_CODE
                       + (TS_CAL1_TEMP * 100);

    return (int16_t)temp_100;
}

/* ── Public API ──────────────────────────────────────────────────────────────
 */

void internal_temp_sensor_init(void)
{
    _last_temp = -1;  /* -1 = invalid/not-ready */
    _last_raw = 0xFFFF;
    _next_tick = HAL_GetTick(); /* Trigger read immediately on first task call */
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
            /* Perform blocking ADC read for temperature sensor */
            uint32_t ts_raw;
            if (adc_read_channel(ADC_CHANNEL_TEMPSENSOR, &ts_raw))
            {
                _last_raw = ts_raw;
                _last_temp = adc_to_celsius_100(ts_raw);
                if (_handler)
                {
                    _handler(InternalTempSensorOk);
                }
            }
            else
            {
                _last_raw = 0xFFFF;
                _last_temp = -1;  /* -1 = error/invalid */
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

int16_t internal_temp_sensor_get(void)
{
    return _last_temp;
}

uint16_t internal_temp_sensor_get_raw(void)
{
    return (uint16_t)_last_raw;
}

void internal_temp_sensor_register_handler(InternalTempHandler handler)
{
    _handler = handler;
}
