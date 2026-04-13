#ifndef BSP_ADC_H
#define BSP_ADC_H

#include "stm32c0xx_hal.h"

/**
 * @brief Initialize ADC1 (internal temperature sensor + VREFINT).
 *        Called by board_init() — do not call directly.
 */
void bsp_adc_init(void);

/**
 * @brief Return the ADC1 HAL handle.
 */
ADC_HandleTypeDef *bsp_adc_get_handle(void);

#endif /* BSP_ADC_H */
