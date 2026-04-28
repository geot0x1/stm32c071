#ifndef SETTINGS_H
#define SETTINGS_H

#include <stdint.h>
#include <stdbool.h>

#define SETTINGS_MAGIC    0x53455454U
#define SETTINGS_VERSION  1

#define SETTINGS_DEFAULT_PWM_THROTTLE_A   50U
#define SETTINGS_DEFAULT_PWM_THROTTLE_B   50U
#define SETTINGS_DEFAULT_TEMP_FAN_ON      3000
#define SETTINGS_DEFAULT_TEMP_FAN_OFF     2500
#define SETTINGS_DEFAULT_TEMP_CRITICAL    6000

typedef enum
{
    FanOverrideAuto   = 0,
    FanOverride2Wire  = 1,
    FanOverride34Wire = 2
} FanTypeOverride;

/*
 * All uint8_t fields are grouped first (8 bytes total including _pad),
 * then int16_t fields, so int16_t members start at an 8-byte aligned offset
 * with no implicit compiler padding needed.
 * Total size: 16 bytes (2 × 8-byte flash write blocks).
 */
typedef struct
{
    uint8_t  version;
    uint8_t  pwm_throttle_a;        /* 0–100 % */
    uint8_t  pwm_throttle_b;        /* 0–100 % */
    uint8_t  fan_type_override[4];  /* FanTypeOverride per unit 0–3 */
    uint8_t  _pad;                  /* explicit pad; int16_t section starts at offset 8 */
    int16_t  temp_fan_on;           /* T_high: fans ON above this, 1/100 °C */
    int16_t  temp_fan_off;          /* T_low:  fans OFF below this, 1/100 °C */
    int16_t  temp_critical;         /* T_critical: overheat threshold, 1/100 °C */
    int16_t  _pad2;                 /* reserved, keeps struct at 16 bytes */
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
bool settings_set_temp_fan_on(int16_t value_centideg);
bool settings_set_temp_fan_off(int16_t value_centideg);
bool settings_set_temp_critical(int16_t value_centideg);
bool settings_set_fan_type_override(uint8_t unit, FanTypeOverride type);

/**
 * @brief Restore all settings to compile-time defaults and persist.
 */
bool settings_reset_to_defaults(void);

#endif /* SETTINGS_H */
