#include "settings.h"
#include "flash.h"
#include "crc32.h"
#include <string.h>

/* First page of the flash storage region is reserved for settings. */
#define SETTINGS_ADDR  FLASH_STORAGE_START_ADDR

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

_Static_assert(sizeof(Settings) == 16U, "Settings struct size changed — update flash layout comment");
_Static_assert(sizeof(SettingsRecord) % 8U == 0U, "SettingsRecord must be a multiple of 8 bytes for flash doubleword writes");

static Settings current;

static const Settings defaults =
{
    .pwm_throttle_a    = SETTINGS_DEFAULT_PWM_THROTTLE_A,
    .pwm_throttle_b    = SETTINGS_DEFAULT_PWM_THROTTLE_B,
    .fan_type_override = {FanOverride2Wire, FanOverride2Wire, FanOverride2Wire, FanOverride2Wire},
    .temp_fan_on       = SETTINGS_DEFAULT_TEMP_FAN_ON,
    .temp_fan_off      = SETTINGS_DEFAULT_TEMP_FAN_OFF,
    .temp_critical     = SETTINGS_DEFAULT_TEMP_CRITICAL,
    ._pad              = {0},
};

static bool settings_save(void)
{
    SettingsRecord record;
    record.magic = SETTINGS_MAGIC;
    record.crc32 = crc32_gen(&current, sizeof(current));
    record.data  = current;

    if (!flash_erase_page(SETTINGS_ADDR))
    {
        return false;
    }
    return flash_write(SETTINGS_ADDR, &record, (uint16_t)sizeof(record));
}

void settings_init(void)
{
    SettingsRecord record;
    bool valid = false;

    if (flash_read(SETTINGS_ADDR, &record, (uint16_t)sizeof(record)))
    {
        if (record.magic == SETTINGS_MAGIC)
        {
            uint32_t computed = crc32_gen(&record.data, sizeof(record.data));
            if (computed == record.crc32)
            {
                bool override_ok = true;
                for (uint8_t i = 0U; i < 4U; i++)
                {
                    if (record.data.fan_type_override[i] > (uint8_t)FanOverride34Wire)
                    {
                        override_ok = false;
                        break;
                    }
                }
                if (override_ok)
                {
                    current = record.data;
                    valid = true;
                }
            }
        }
    }

    if (!valid)
    {
        current = defaults;
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

bool settings_set_temp_fan_on(int16_t value_centideg)
{
    if (value_centideg <= current.temp_fan_off || value_centideg >= current.temp_critical)
    {
        return false;
    }
    current.temp_fan_on = value_centideg;
    return settings_save();
}

bool settings_set_temp_fan_off(int16_t value_centideg)
{
    if (value_centideg >= current.temp_fan_on)
    {
        return false;
    }
    current.temp_fan_off = value_centideg;
    return settings_save();
}

bool settings_set_temp_critical(int16_t value_centideg)
{
    if (value_centideg <= current.temp_fan_on)
    {
        return false;
    }
    current.temp_critical = value_centideg;
    return settings_save();
}

bool settings_set_fan_type_override(uint8_t unit, FanTypeOverride type)
{
    if (unit >= 4U)
    {
        return false;
    }
    if (type != FanOverride2Wire && type != FanOverride34Wire)
    {
        return false;
    }
    current.fan_type_override[unit] = (uint8_t)type;
    return settings_save();
}

bool settings_reset_to_defaults(void)
{
    current = defaults;
    return settings_save();
}
