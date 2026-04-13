#ifndef BOARD_H
#define BOARD_H

#include <stdbool.h>
#include "stm32c0xx_hal.h"
#include "i2c.h"
#include "bsp_usb.h"

/**
 * @brief Initialize board hardware (clocks, GPIO, I2C, USB, FLASH).
 *
 * Call once at startup after HAL_Init(). Timer initialization is handled
 * separately by timers_init().
 */
void board_init(void);

/* ── LED ─────────────────────────────────────────────────────────────────── */

/** @brief Drive the status LED. true = ON, false = OFF. */
void board_led_set(bool on);

/** @brief Toggle the status LED. */
void board_led_toggle(void);

/* ── Peripheral getters ──────────────────────────────────────────────────── */

/** @brief I2C sensor bus handle. */
I2c_t *board_get_i2c(void);

/** @brief USB PCD handle (for power management / future use) */
Usb_t *board_get_usb(void);

/** @brief ADC1 handle (internal temperature sensor + VREFINT). */
ADC_HandleTypeDef *board_get_adc(void);

/* ── Error handler ───────────────────────────────────────────────────────── */

void Error_Handler(void);

#endif /* BOARD_H */
