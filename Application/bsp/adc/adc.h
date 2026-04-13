#ifndef ADC_H
#define ADC_H

#include "stm32c0xx_hal.h"
#include <stdint.h>
#include <stdbool.h>

/**
 * @brief Initialize ADC1 (internal temperature sensor + VREFINT).
 *        Called by board_init() — do not call directly.
 */
void adc_init(void);

/**
 * @brief Return the ADC1 HAL handle.
 */
ADC_HandleTypeDef *adc_get_handle(void);

/**
 * @brief Read a single ADC channel (blocking).
 *
 * @param channel The ADC channel to read (e.g., ADC_CHANNEL_TEMPSENSOR)
 * @param out_raw Pointer to store the 12-bit raw ADC value
 * @return true on success, false on timeout/error
 */
bool adc_read_channel(uint32_t channel, uint32_t *out_raw);

#endif /* ADC_H */
