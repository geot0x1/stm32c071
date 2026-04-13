#include "internal_temp_sensor.h"
#include "adc.h"
#include "stm32c0xx_hal.h"
#include "stm32c0xx_ll_adc.h"

/* ── Factory calibration constants ───────────────────────────────────────────
 * STM32C071 internal temperature sensor calibration values stored in Flash.
 * Source: STM32C0 reference manual (RM0490), Section 16.3.6, page 321
 *
 * STM32C0 has ONE factory calibration point at 30°C stored at 0x1FFF7568.
 * VREFINT calibration (measured at Vdda=3.0V) is stored at 0x1FFF756A.
 * Temperature is calculated using the formula from RM0490, page 321:
 *   Temperature (°C) = (TS_DATA - TS_CAL1) / Avg_Slope_Code + T_CAL1
 *
 * The STM32C071 has a POSITIVE temperature coefficient (+2.5 mV/°C typical).
 * As the chip heats up, the sensor voltage (and ADC count) increases.
 * This differs from older STM32F1 which have negative coefficients (-4.3 mV/°C).
 *
 * Critical: Both calibrations (TS_CAL1 and VREFINT_CAL) were performed at
 * Vdda = 3.0V. The ADC raw counts scale with actual supply voltage. To ensure
 * accuracy across varying Vdda, we dynamically measure actual Vdda using
 * VREFINT, then normalize temperature sensor data to the 3.0V calibration point.
 *
 * Vdda Correction Formula:
 *   Vdda_mV = 3000 × (*VREFINT_CAL) / vrefint_raw
 *   corrected = ts_raw × Vdda_mV / 3000
 *   T = (corrected - TS_CAL1) / TS_AVGSLOPE_CODE + 30°C
 */
#define TS_CAL1_ADDR        ((const uint16_t *)0x1FFF7568UL) /* TS_CAL1: raw ADC @ 30°C, Vdda=3.0V */
#define TS_CAL1_TEMP        30
#define TS_AVG_SLOPE_UV     (2500)   /* Avg_Slope: +2.5 mV/°C (positive: ADC increases as temp rises) */
#define TS_VREF_CAL_MV      (3000)   /* Calibration Vdda reference: 3.0V */
/* VREFINT_CAL_ADDR is defined in stm32c0xx_ll_adc.h; no need to redefine here */
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
 * @brief Convert ADC raw values to Celsius * 100 using dynamic Vdda correction.
 *
 * Uses the single-point calibration method from STM32C0 reference manual (RM0490, page 321),
 * with dynamic Vdda measurement via VREFINT for improved accuracy.
 *
 * Procedure:
 *   1. Measure actual Vdda using VREFINT factory calibration:
 *      Vdda_mV = 3000 × (*VREFINT_CAL) / vrefint_raw
 *   2. Normalize ts_raw to 3.0V calibration reference:
 *      corrected = ts_raw × (Vdda_mV / 3000)
 *   3. Apply single-point calibration formula:
 *      T × 100 = (corrected - TS_CAL1) × 100000 / TS_AVGSLOPE_CODE + 30 × 100
 *
 * The Vdda correction is essential: factory calibration was at 3.0V. ADC counts scale
 * with actual supply voltage. Without correction, temperature error can be ±5°C or more
 * if Vdda drifts. Dynamic VREFINT measurement provides accurate correction.
 *
 * Overflow check (int32_t safety):
 *   Vdda_mV: max 3600 (if Vdda = 3.6V)
 *   corrected: max 4095 × 3600 / 3000 = 4914, fits in int32
 *   (corrected - cal1) × 100000: max 3400 × 100000 = 340M < INT32_MAX ✓
 *
 * @param ts_raw TEMPSENSOR raw ADC value (12-bit, 0-4095)
 * @param vrefint_raw VREFINT raw ADC value (12-bit, 0-4095)
 * @return Temperature in Celsius * 100 (e.g., 2730 = 27.30°C), or -1 on error
 */
static int16_t adc_to_celsius_100(uint32_t ts_raw, uint32_t vrefint_raw)
{
    /* Step 1: Compute actual Vdda using VREFINT measurement.
     * VREFINT_CAL_ADDR holds factory ADC count at Vdda=3.0V.
     * Vdda_mV = 3000 × VREFINT_CAL / vrefint_raw
     */
    int32_t vdda_mv = ((int32_t)TS_VREF_CAL_MV * (int32_t)(*VREFINT_CAL_ADDR))
                      / (int32_t)vrefint_raw;

    /* Step 2: Normalize ts_raw to 3.0V calibration reference.
     * corrected = ts_raw × (Vdda / 3.0V)
     */
    int32_t corrected_ts = ((int32_t)ts_raw * vdda_mv) / (int32_t)TS_VREF_CAL_MV;

    /* Step 3: Single-point calibration formula (RM0490, Section 16.3.6).
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
            /* Perform blocking ADC reads for both temperature sensor and VREFINT.
             * VREFINT is used to dynamically measure actual Vdda for accurate
             * temperature correction.
             */
            uint32_t ts_raw, vrefint_raw;
            bool ts_ok = adc_read_channel(ADC_CHANNEL_TEMPSENSOR, &ts_raw);
            bool vref_ok = adc_read_channel(ADC_CHANNEL_VREFINT, &vrefint_raw);

            if (ts_ok && vref_ok && vrefint_raw != 0)
            {
                _last_raw = ts_raw;
                _last_temp = adc_to_celsius_100(ts_raw, vrefint_raw);
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
