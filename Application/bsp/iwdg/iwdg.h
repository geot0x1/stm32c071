#ifndef BSP_IWDG_H
#define BSP_IWDG_H

#include <stdint.h>
#include "stm32c0xx_hal.h"

typedef struct Iwdg_s
{
    IWDG_HandleTypeDef hal_handle;
} Iwdg_t;

typedef enum
{
    IWDG_OK = 0,
    IWDG_ERR_HAL
} Iwdg_err_t;

/**
 * @brief Initialize the IWDG peripheral.
 *
 * @param iwdg       Handle to fill.
 * @param prescaler  Clock prescaler — one of IWDG_PRESCALER_x (e.g. IWDG_PRESCALER_32).
 * @param reload     Reload counter value (0–4095).
 * @param window     Window value (0–4095; set to 4095 to disable windowing).
 */
void iwdg_init(Iwdg_t *iwdg, uint32_t prescaler, uint32_t reload, uint32_t window);

/**
 * @brief Refresh (kick) the watchdog to prevent a reset.
 */
Iwdg_err_t iwdg_refresh(Iwdg_t *iwdg);

#endif /* BSP_IWDG_H */
