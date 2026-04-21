#ifndef BSP_LPTIM_H
#define BSP_LPTIM_H

/**
 * @file lptim.h
 * @brief BSP driver for LPTIM1 — Low-Power Timer
 *
 * LPTIM1 is a separate peripheral from the general-purpose TIM timers.
 * It uses a different HAL (stm32c0xx_hal_lptim.h) and a different handle
 * type (LPTIM_HandleTypeDef). It is NOT part of tim.h/tim.c.
 *
 * Key differences vs TIM peripherals:
 *   - Can run from LSI (~32 kHz) or LSE (32.768 kHz) during CPU Stop/Standby
 *   - Single 16-bit counter with limited prescaler options (1, 2, 4, 8, 16, 32, 64, 128)
 *   - Single output channel (PA2 AF5 or PB5 AF5) and single input channel
 *   - Primarily useful for periodic wake-up from low-power modes
 *
 * PREREQUISITES — this module requires:
 *   1. Copy stm32c0xx_hal_lptim.c from STM32CubeC0 package to:
 *         Drivers/STM32C0xx_HAL_Driver/Src/stm32c0xx_hal_lptim.c
 *   2. Copy stm32c0xx_hal_lptim.h to:
 *         Drivers/STM32C0xx_HAL_Driver/Inc/stm32c0xx_hal_lptim.h
 *   3. In Core/Inc/stm32c0xx_hal_conf.h, uncomment:
 *         #define HAL_LPTIM_MODULE_ENABLED
 *   4. In cmake/stm32cubemx/CMakeLists.txt, add to STM32_Drivers_Src:
 *         .../Drivers/STM32C0xx_HAL_Driver/Src/stm32c0xx_hal_lptim.c
 *   5. In Application/bsp/lptim/CMakeLists.txt, add bsp_lptim target and
 *      link it to the main executable via CMakeLists.txt at project root.
 *   6. In Application/timers/timers.h, add: Lptim *timers_get_lptim(void);
 *   7. In Application/timers/timers.c, add init call and getter.
 */

#include <stdint.h>
#include "stm32c0xx_hal.h"

/**
 * @brief LPTIM handle
 *
 * Separate from Tim — LPTIM uses LPTIM_HandleTypeDef, not TIM_HandleTypeDef.
 */
typedef struct Lptim_s {
    LPTIM_HandleTypeDef hal_handle;
    uint32_t tick_hz;
} Lptim;

/**
 * @brief Initialize LPTIM1 as a free-running counter (polling mode)
 *
 * Configures LPTIM1 with PCLK as clock source and selects the prescaler
 * to achieve the closest possible tick rate to tick_hz.
 * Starts the counter in continuous mode (wraps at 0xFFFF).
 *
 * @param lptim     LPTIM handle (allocated by caller)
 * @param tick_hz   Desired counting frequency in Hz
 *                  (actual rate may differ due to prescaler quantization)
 *
 * @note For low-power wake-up use, switch the clock source to LSI or LSE
 *       via HAL_RCCEx_PeriphCLKConfig() before calling this function.
 */
void lptim_base_init(Lptim *lptim, uint32_t tick_hz);

/**
 * @brief Get current LPTIM1 counter value
 *
 * @param lptim     LPTIM handle
 * @return          16-bit counter value (0–0xFFFF)
 */
uint32_t lptim_get_count(Lptim *lptim);

/**
 * @brief Start LPTIM1 with compare-match interrupt
 *
 * Enables LPTIM1 in continuous mode and sets the compare register.
 * HAL_LPTIM_CompareMatchCallback() is called each time CNT == compare.
 * The counter keeps running — to get periodic callbacks, reload the
 * compare value inside the callback.
 *
 * @param lptim     LPTIM handle
 * @param compare   Compare value (0–0xFFFF); interrupt fires when CNT equals this
 *
 * @note NVIC for LPTIM1_IRQn is configured in HAL_LPTIM_MspInit()
 *       in stm32c0xx_hal_msp.c.
 */
void lptim_start_it(Lptim *lptim, uint32_t compare);

/**
 * @brief Stop LPTIM1 and disable compare interrupt
 *
 * @param lptim     LPTIM handle
 */
void lptim_stop_it(Lptim *lptim);

#endif // BSP_LPTIM_H
