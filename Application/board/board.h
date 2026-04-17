#ifndef BOARD_H
#define BOARD_H

#include <stdbool.h>
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

/* ── Power enable outputs ────────────────────────────────────────────────── */

/** @brief Enable/disable DS18B20 VDD supply (PB5 → Q2). */
void board_onewire_power_set(bool on);

/** @brief Enable/disable 1-Wire strong pull-up resistor (PB8 → Q3). */
void board_onewire_pullup_set(bool on);

/** @brief Enable/disable LCD backlight power rail (PB15 → Q18). */
void board_lcd_power_set(bool on);

/* ── Peripheral getters ──────────────────────────────────────────────────── */

/** @brief I2C sensor bus handle. */
I2c_t *board_get_i2c(void);

/** @brief USB PCD handle (for power management / future use) */
Usb_t *board_get_usb(void);

/* ── Error handler ───────────────────────────────────────────────────────── */

void Error_Handler(void);

#endif /* BOARD_H */
