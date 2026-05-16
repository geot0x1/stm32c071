#include "settings.h"
#include "flash.h"
#include "crc32.h"
#include <string.h>

/* First page of the flash storage region is reserved for settings. */
#define SETTINGS_ADDR FLASH_STORAGE_START_ADDR

/*
 * On-flash record layout:
 *   magic  (4 bytes) — sentinel to detect blank/garbage flash
 *   crc32  (4 bytes) — CRC32 over the Settings struct
 *   data   (16 bytes) — Settings struct
 * Total: 24 bytes = 3 × 8-byte flash write blocks.
 */
typedef struct
{
    uint32_t magic;
    uint32_t crc32;
    Settings data;
} SettingsRecord;

_Static_assert(
    sizeof(Settings) == 16U, "Settings struct size changed — update flash layout comment");
_Static_assert(sizeof(SettingsRecord) % 8U == 0U,
    "SettingsRecord must be a multiple of 8 bytes for flash doubleword writes");

_Static_assert(CONFIG_TEMP_FAN_OFF_DEFAULT < CONFIG_TEMP_FAN_ON_DEFAULT,
    "CONFIG: temp_fan_off must be below temp_fan_on");
_Static_assert(CONFIG_TEMP_FAN_ON_DEFAULT < CONFIG_TEMP_THROTTLE_ON_DEFAULT,
    "CONFIG: temp_fan_on must be below temp_throttle_on");
_Static_assert(CONFIG_TEMP_THROTTLE_ON_DEFAULT < CONFIG_TEMP_CRITICAL_DEFAULT,
    "CONFIG: temp_throttle_on must be below temp_critical");
_Static_assert(CONFIG_TEMP_CRITICAL_DEFAULT <= CONFIG_TEMP_MAX,
    "CONFIG: temp_critical exceeds 180 °C limit");
_Static_assert(CONFIG_TEMP_FAN_OFF_DEFAULT <= CONFIG_TEMP_MAX,
    "CONFIG: temp_fan_off exceeds 180 °C limit");

static Settings current;

static const Settings DEFAULTS = {
    .pwm_throttle_a = SETTINGS_DEFAULT_PWM_THROTTLE_A,
    .pwm_throttle_b = SETTINGS_DEFAULT_PWM_THROTTLE_B,
    .temp_throttle_on = SETTINGS_DEFAULT_TEMP_THROTTLE_ON,
    .temp_fan_on = SETTINGS_DEFAULT_TEMP_FAN_ON,
    .temp_fan_off = SETTINGS_DEFAULT_TEMP_FAN_OFF,
    .temp_critical = SETTINGS_DEFAULT_TEMP_CRITICAL,
    ._pad = {0},
};

static bool settings_save(void)
{
    SettingsRecord record;
    record.magic = SETTINGS_MAGIC;
    record.crc32 = crc32_gen(&current, sizeof(current));
    record.data = current;

    if (!flash_erase_page(SETTINGS_ADDR))
    {
        return false;
    }
    return flash_write(SETTINGS_ADDR, &record, (uint16_t)sizeof(record));
}

static bool settings_record_has_erased_temps(const Settings *settings)
{
    return settings->temp_throttle_on == SETTINGS_TEMP_INVALID
        || settings->temp_fan_on == SETTINGS_TEMP_INVALID
        || settings->temp_fan_off == SETTINGS_TEMP_INVALID
        || settings->temp_critical == SETTINGS_TEMP_INVALID;
}

void settings_init(void)
{
    SettingsRecord record;
    bool valid = false;

    if (flash_read(SETTINGS_ADDR, &record, (uint16_t)sizeof(record)))
    {
        if (record.magic == SETTINGS_MAGIC)
        {
            if (settings_record_has_erased_temps(&record.data))
            {
                valid = false;
            }
            else
            {
                uint32_t computed = crc32_gen(&record.data, sizeof(record.data));
                if (computed == record.crc32)
                {
                    current = record.data;
                    valid = true;
                }
            }
        }
    }

    if (!valid)
    {
        current = DEFAULTS;
        settings_save();
    }
}

const Settings *settings_get(void)
{
    return &current;
}

bool settings_set_pwm_throttle_a(uint8_t percent)
{
    if (percent > 100U)
    {
        return false;
    }
    current.pwm_throttle_a = percent;
    return settings_save();
}

bool settings_set_pwm_throttle_b(uint8_t percent)
{
    if (percent > 100U)
    {
        return false;
    }
    current.pwm_throttle_b = percent;
    return settings_save();
}

bool settings_set_temp_throttle_on(uint8_t temp_deg)
{
    if (temp_deg == SETTINGS_TEMP_INVALID
        || temp_deg <= current.temp_fan_off
        || temp_deg >= current.temp_critical)
    {
        return false;
    }
    current.temp_throttle_on = temp_deg;
    return settings_save();
}

bool settings_set_temp_fan_on(uint8_t temp_deg)
{
    if (temp_deg == SETTINGS_TEMP_INVALID
        || temp_deg <= current.temp_fan_off
        || temp_deg >= current.temp_critical)
    {
        return false;
    }
    current.temp_fan_on = temp_deg;
    return settings_save();
}

bool settings_set_temp_fan_off(uint8_t temp_deg)
{
    if (temp_deg == SETTINGS_TEMP_INVALID || temp_deg >= current.temp_fan_on)
    {
        return false;
    }
    current.temp_fan_off = temp_deg;
    return settings_save();
}

bool settings_set_temp_critical(uint8_t temp_deg)
{
    if (temp_deg == SETTINGS_TEMP_INVALID || temp_deg <= current.temp_fan_on)
    {
        return false;
    }
    current.temp_critical = temp_deg;
    return settings_save();
}

bool settings_set_all(const Settings *s)
{
    if (s == NULL)
    {
        return false;
    }
    if (s->temp_fan_off    == SETTINGS_TEMP_INVALID
        || s->temp_fan_on     == SETTINGS_TEMP_INVALID
        || s->temp_throttle_on == SETTINGS_TEMP_INVALID
        || s->temp_critical   == SETTINGS_TEMP_INVALID)
    {
        return false;
    }
    if (s->temp_fan_off >= s->temp_fan_on
        || s->temp_fan_on >= s->temp_throttle_on
        || s->temp_throttle_on >= s->temp_critical)
    {
        return false;
    }
    if ((s->pwm_throttle_a > 100U) || (s->pwm_throttle_b > 100U))
    {
        return false;
    }
    current = *s;
    return settings_save();
}

bool settings_reset_to_defaults(void)
{
    current = DEFAULTS;
    return settings_save();
}
