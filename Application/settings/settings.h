#ifndef SETTINGS_H
#define SETTINGS_H

#include "config.h"
#include <stdint.h>
#include <stdbool.h>

#define SETTINGS_MAGIC 0x53455454U

#define SETTINGS_DEFAULT_TEMP_CRITICAL    CONFIG_TEMP_CRITICAL_DEFAULT
#define SETTINGS_DEFAULT_TEMP_THROTTLE_ON CONFIG_TEMP_THROTTLE_ON_DEFAULT
#define SETTINGS_DEFAULT_TEMP_FAN_ON      CONFIG_TEMP_FAN_ON_DEFAULT
#define SETTINGS_DEFAULT_TEMP_FAN_OFF     CONFIG_TEMP_FAN_OFF_DEFAULT
#define SETTINGS_DEFAULT_PWM_THROTTLE_A   CONFIG_PWM_THROTTLE_A_DEFAULT
#define SETTINGS_DEFAULT_PWM_THROTTLE_B   CONFIG_PWM_THROTTLE_B_DEFAULT
#define SETTINGS_TEMP_INVALID 255U

/* Total size: 16 bytes (2 × 8-byte flash write blocks). */
typedef struct
{
    uint8_t pwm_throttle_a; /* offset  0 — 0–100 % */
    uint8_t pwm_throttle_b; /* offset  1 — 0–100 % */
    uint8_t temp_throttle_on; /* offset  2 — T_throttle: throttle above this, 0–254 °C */
    uint8_t temp_fan_on; /* offset  3 — T_high: fans ON above this, 0–254 °C */
    uint8_t temp_fan_off; /* offset  4 — T_low:  fans OFF below this, 0–254 °C */
    uint8_t temp_critical; /* offset  5 — T_critical: overheat threshold, 0–254 °C */
    uint8_t _pad[10]; /* offset  6 — pads struct to 16 bytes */
} Settings;

/**
 * @brief Load settings from flash on boot.
 *        Falls back to compile-time defaults and writes them if flash is blank or CRC fails.
 */
void settings_init(void);

/**
 * @brief Return a read-only pointer to the live settings struct.
 */
const Settings *settings_get(void);

/**
 * @brief Setters — validate input, update the live struct, and persist to flash.
 * @return true on success; false if validation fails or flash write fails.
 */
bool settings_set_pwm_throttle_a(uint8_t percent);
bool settings_set_pwm_throttle_b(uint8_t percent);
bool settings_set_temp_throttle_on(uint8_t temp_deg);
bool settings_set_temp_fan_on(uint8_t temp_deg);
bool settings_set_temp_fan_off(uint8_t temp_deg);
bool settings_set_temp_critical(uint8_t temp_deg);

/**
 * @brief Restore all settings to compile-time defaults and persist.
 */
bool settings_reset_to_defaults(void);

#endif /* SETTINGS_H */
