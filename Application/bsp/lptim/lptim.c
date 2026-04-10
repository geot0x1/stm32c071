#include "lptim.h"
#include <string.h>

/**
 * @brief Select LPTIM prescaler to approximate tick_hz from PCLK
 *
 * LPTIM prescaler options: /1, /2, /4, /8, /16, /32, /64, /128
 * Returns the HAL prescaler constant and actual tick_hz via out_actual_hz.
 */
static uint32_t lptim_select_prescaler(uint32_t clk_freq, uint32_t tick_hz,
                                       uint32_t *out_actual_hz)
{
    static const uint32_t divs[]  = {1, 2, 4, 8, 16, 32, 64, 128};
    static const uint32_t consts[] = {
        LPTIM_PRESCALER_DIV1,  LPTIM_PRESCALER_DIV2,
        LPTIM_PRESCALER_DIV4,  LPTIM_PRESCALER_DIV8,
        LPTIM_PRESCALER_DIV16, LPTIM_PRESCALER_DIV32,
        LPTIM_PRESCALER_DIV64, LPTIM_PRESCALER_DIV128
    };

    uint32_t best_const = LPTIM_PRESCALER_DIV1;
    uint32_t best_hz = clk_freq;
    uint32_t best_diff = (clk_freq > tick_hz) ? (clk_freq - tick_hz) : (tick_hz - clk_freq);

    for (uint8_t i = 0; i < 8; i++) {
        uint32_t hz = clk_freq / divs[i];
        uint32_t diff = (hz > tick_hz) ? (hz - tick_hz) : (tick_hz - hz);
        if (diff < best_diff) {
            best_diff = diff;
            best_hz = hz;
            best_const = consts[i];
        }
    }

    *out_actual_hz = best_hz;
    return best_const;
}

void lptim_base_init(Lptim_t *lptim, uint32_t tick_hz)
{
    memset(lptim, 0, sizeof(Lptim_t));

    uint32_t clk_freq = HAL_RCC_GetPCLK1Freq();
    uint32_t actual_hz;
    uint32_t prescaler = lptim_select_prescaler(clk_freq, tick_hz, &actual_hz);

    lptim->tick_hz = actual_hz;
    lptim->hal_handle.Instance = LPTIM1;
    lptim->hal_handle.Init.Clock.Source    = LPTIM_CLOCKSOURCE_APBCLOCK_LPOSC;
    lptim->hal_handle.Init.Clock.Prescaler = prescaler;
    lptim->hal_handle.Init.UltraLowPowerClock.Polarity  = LPTIM_CLOCKPOLARITY_RISING;
    lptim->hal_handle.Init.UltraLowPowerClock.SampleTime = LPTIM_CLOCKSAMPLETIME_DIRECTTRANSITION;
    lptim->hal_handle.Init.Trigger.Source   = LPTIM_TRIGSOURCE_SOFTWARE;
    lptim->hal_handle.Init.OutputPolarity   = LPTIM_OUTPUTPOLARITY_HIGH;
    lptim->hal_handle.Init.UpdateMode       = LPTIM_UPDATE_IMMEDIATE;
    lptim->hal_handle.Init.CounterSource    = LPTIM_COUNTERSOURCE_INTERNAL;
    lptim->hal_handle.Init.Input1Source     = LPTIM_INPUT1SOURCE_GPIO;
    lptim->hal_handle.Init.Input2Source     = LPTIM_INPUT2SOURCE_GPIO;

    HAL_LPTIM_Init(&lptim->hal_handle);
    HAL_LPTIM_Counter_Start(&lptim->hal_handle, 0xFFFF);
}

uint32_t lptim_get_count(Lptim_t *lptim)
{
    return HAL_LPTIM_ReadCounter(&lptim->hal_handle);
}

void lptim_start_it(Lptim_t *lptim, uint32_t compare)
{
    HAL_LPTIM_Counter_Stop(&lptim->hal_handle);
    HAL_LPTIM_SetOnce_Start_IT(&lptim->hal_handle, 0xFFFF, compare & 0xFFFF);
}

void lptim_stop_it(Lptim_t *lptim)
{
    HAL_LPTIM_SetOnce_Stop_IT(&lptim->hal_handle);
}
