#include "board.h"
#include "delay.h"
#include "fan_control.h"
#include "fan_tacho.h"
#include "flash.h"
#include "pwm_repeater.h"
#include "settings.h"
#include "commands.h"
#include "stm32c0xx_hal.h"
#include "temperature_sensor.h"
#include "timers/timers.h"
#include "usb.h"
#include "serial.h"
#include "board/board_config.h"
#include "watchdog.h"
#include <stdint.h>
#include <string.h>

static void flash_test(void)
{
    static const uint8_t patterns[2] = {0xA5U, 0x5AU};

    serial_printf("FLASH TEST: start\r\n");

    for (uint8_t p = 0; p < FLASH_STORAGE_SECTOR_COUNT; p++)
    {
        uint32_t addr = FLASH_STORAGE_START_ADDR + (uint32_t)p * FLASH_STORAGE_SECTOR_SIZE;
        if (!flash_erase_page(addr))
        {
            serial_printf("FLASH TEST: initial erase page %u FAILED\r\n", (unsigned int)p);
            return;
        }
    }

    for (uint8_t run = 0; run < 2U; run++)
    {
        uint8_t pat = patterns[run];
        serial_printf(
            "FLASH TEST: run %u write (0x%02X)\r\n", (unsigned int)(run + 1U), (unsigned int)pat);

        uint8_t chunk[8];
        memset(chunk, pat, sizeof(chunk));

        for (uint8_t p = 0; p < FLASH_STORAGE_SECTOR_COUNT; p++)
        {
            uint32_t page_base = FLASH_STORAGE_START_ADDR + (uint32_t)p * FLASH_STORAGE_SECTOR_SIZE;
            for (uint16_t off = 0; off < FLASH_STORAGE_SECTOR_SIZE; off += 8U)
            {
                if (!flash_write(page_base + off, chunk, 8U))
                {
                    serial_printf("FLASH TEST: run %u write page %u off %u FAILED\r\n",
                        (unsigned int)(run + 1U), (unsigned int)p, (unsigned int)off);
                    return;
                }
            }
        }

        serial_printf("FLASH TEST: run %u verify\r\n", (unsigned int)(run + 1U));
        bool pass = true;
        for (uint8_t p = 0; p < FLASH_STORAGE_SECTOR_COUNT && pass; p++)
        {
            uint32_t page_base = FLASH_STORAGE_START_ADDR + (uint32_t)p * FLASH_STORAGE_SECTOR_SIZE;
            for (uint16_t off = 0; off < FLASH_STORAGE_SECTOR_SIZE && pass; off += 8U)
            {
                uint8_t buf[8];
                if (!flash_read(page_base + off, buf, 8U))
                {
                    serial_printf("FLASH TEST: run %u read page %u off %u FAILED\r\n",
                        (unsigned int)(run + 1U), (unsigned int)p, (unsigned int)off);
                    pass = false;
                    break;
                }
                for (uint8_t b = 0; b < 8U && pass; b++)
                {
                    if (buf[b] != pat)
                    {
                        serial_printf("FLASH TEST: run %u mismatch page %u byte %u: got 0x%02X exp "
                                      "0x%02X\r\n",
                            (unsigned int)(run + 1U), (unsigned int)p, (unsigned int)(off + b),
                            (unsigned int)buf[b], (unsigned int)pat);
                        pass = false;
                    }
                }
            }
        }

        serial_printf(
            "FLASH TEST: run %u %s\r\n", (unsigned int)(run + 1U), pass ? "PASS" : "FAIL");

        serial_printf("FLASH TEST: run %u erase\r\n", (unsigned int)(run + 1U));
        for (uint8_t p = 0; p < FLASH_STORAGE_SECTOR_COUNT; p++)
        {
            uint32_t addr = FLASH_STORAGE_START_ADDR + (uint32_t)p * FLASH_STORAGE_SECTOR_SIZE;
            if (!flash_erase_page(addr))
            {
                serial_printf("FLASH TEST: run %u erase page %u FAILED\r\n",
                    (unsigned int)(run + 1U), (unsigned int)p);
                return;
            }
        }

        watchdog_kick();
    }

    serial_printf("FLASH TEST: done\r\n");
}

void temperature_sensor_event_handler(TempSensorEvent event)
{
    switch (event)
    {
        case SensorLost:
            usb_printf("TEMP: SENSOR LOST\r\n");
            break;
        case AboveA:
            usb_printf("TEMP: ABOVE A\r\n");
            break;
        case AboveB:
            usb_printf("TEMP: ABOVE B\r\n");
            break;
        case BelowA:
            usb_printf("TEMP: BELOW A\r\n");
            break;
        case BelowB:
            usb_printf("TEMP: BELOW B\r\n");
            break;
    }
}

static void flash_debug_addresses(void)
{
    uint32_t storage_start = FLASH_STORAGE_START_ADDR;
    uint32_t storage_end   = FLASH_STORAGE_START_ADDR +
                             ((uint32_t)FLASH_STORAGE_SECTOR_COUNT * FLASH_STORAGE_SECTOR_SIZE);
    uint32_t settings_size = sizeof(Settings) + 8U; /* magic(4) + crc32(4) + Settings */

    serial_printf("---- Flash Address Map ----\r\n");
    serial_printf("  FLASH_BASE             : 0x%08X\r\n", (unsigned int)FLASH_BASE);
    serial_printf("  storage start          : 0x%08X\r\n", (unsigned int)storage_start);
    serial_printf("  storage end            : 0x%08X\r\n", (unsigned int)storage_end);
    serial_printf("  storage size           : %u bytes (%u sectors x %u bytes)\r\n",
        (unsigned int)((uint32_t)FLASH_STORAGE_SECTOR_COUNT * FLASH_STORAGE_SECTOR_SIZE),
        (unsigned int)FLASH_STORAGE_SECTOR_COUNT,
        (unsigned int)FLASH_STORAGE_SECTOR_SIZE);
    serial_printf("  page range             : %u - %u\r\n",
        (unsigned int)((storage_start - FLASH_BASE) / FLASH_STORAGE_SECTOR_SIZE),
        (unsigned int)((storage_end - FLASH_BASE) / FLASH_STORAGE_SECTOR_SIZE - 1U));

    for (uint8_t i = 0U; i < FLASH_STORAGE_SECTOR_COUNT; i++)
    {
        uint32_t s = storage_start + (uint32_t)i * FLASH_STORAGE_SECTOR_SIZE;
        serial_printf("  sector[%u]              : 0x%08X - 0x%08X\r\n",
            (unsigned int)i, (unsigned int)s, (unsigned int)(s + FLASH_STORAGE_SECTOR_SIZE));
    }

    serial_printf("  settings addr          : 0x%08X\r\n", (unsigned int)storage_start);
    serial_printf("  settings record size   : %u bytes\r\n", (unsigned int)settings_size);

    bool in_bounds = (settings_size <= FLASH_STORAGE_SECTOR_SIZE);
    bool aligned   = ((storage_start & 0x7U) == 0U);
    serial_printf("  settings in sector[0]  : %s\r\n", in_bounds ? "PASS" : "FAIL");
    serial_printf("  settings 8-byte aligned: %s\r\n", aligned ? "PASS" : "FAIL");
    serial_printf("---------------------------\r\n");
}

static void settings_test_print(void)
{
    static const char *const fan_names[] = {"Auto", "2Wire", "3/4Wire"};
    const Settings *s = settings_get();
    serial_printf("---- Settings ----\r\n");
    serial_printf("  pwm_throttle_a   : %u%%\r\n", (unsigned int)s->pwm_throttle_a);
    serial_printf("  pwm_throttle_b   : %u%%\r\n", (unsigned int)s->pwm_throttle_b);
    for (uint8_t i = 0U; i < 4U; i++)
    {
        uint8_t ov = s->fan_type_override[i];
        serial_printf("  fan_override[%u]  : %s\r\n", (unsigned int)i,
            (ov < 3U) ? fan_names[ov] : "?");
    }
    serial_printf("  temp_fan_on      : %d centideg\r\n", (int)s->temp_fan_on);
    serial_printf("  temp_fan_off     : %d centideg\r\n", (int)s->temp_fan_off);
    serial_printf("  temp_critical    : %d centideg\r\n", (int)s->temp_critical);
    serial_printf("------------------\r\n");
}

int main(void)
{
    HAL_Init();

    board_init();
    timers_init();
    delay_init(timers_get_sys_timer());

    watchdog_init();

    usb_init();
    serial_init(BOARD_UART1_INSTANCE, BOARD_UART1_BAUD_RATE);

    serial_printf("Program started\r\n");

    flash_debug_addresses();

    settings_init();
    commands_init();
    const Settings *s = settings_get();

    // while (true)
    // {
    //     watchdog_kick();
    //     flash_test();
    // }

    pwm_repeater_init(timers_get_capture(), timers_get_repeater_a(), timers_get_repeater_b());
    pwm_set_throttle_a((uint32_t)s->pwm_throttle_a);
    pwm_set_throttle_b((uint32_t)s->pwm_throttle_b);
    board_onewire_power_set(true);
    board_onewire_pullup_set(true);
    temperature_sensor_init();
    temperature_sensor_set_setpoint_a(s->temp_fan_off);
    temperature_sensor_set_setpoint_b(s->temp_fan_on);
    fan_tacho_init(1);
    fan_tacho_init(2);
    fan_tacho_init(3);
    fan_tacho_init(4);

    fan_tacho_enable(1);
    fan_tacho_enable(2);
    fan_tacho_enable(3);
    fan_tacho_enable(4);

    static uint32_t last_step = 0;
    static uint8_t step = 0;
    bool first = true;

    while (true)
    {
        watchdog_kick();
        usb_task();
        commands_task();

        if (first)
        {
            first = false;
            serial_printf("SETTINGS TEST: initial state\r\n");
            settings_test_print();
            last_step = HAL_GetTick();
        }

        if (HAL_GetTick() - last_step >= 5000U)
        {
            last_step = HAL_GetTick();
            bool ok = false;

            switch (step)
            {
                case 0U:
                    ok = settings_set_pwm_throttle_a(75U);
                    serial_printf("SETTINGS TEST: set pwm_throttle_a=75 -> %s\r\n", ok ? "OK" : "FAIL");
                    break;
                case 1U:
                    ok = settings_set_pwm_throttle_b(30U);
                    serial_printf("SETTINGS TEST: set pwm_throttle_b=30 -> %s\r\n", ok ? "OK" : "FAIL");
                    break;
                case 2U:
                    ok = settings_set_fan_type_override(0U, FanOverride2Wire);
                    serial_printf("SETTINGS TEST: set fan_override[0]=2Wire -> %s\r\n", ok ? "OK" : "FAIL");
                    break;
                case 3U:
                    ok = settings_set_fan_type_override(1U, FanOverride34Wire);
                    serial_printf("SETTINGS TEST: set fan_override[1]=3/4Wire -> %s\r\n", ok ? "OK" : "FAIL");
                    break;
                case 4U:
                    ok = settings_set_temp_fan_on(3500);
                    serial_printf("SETTINGS TEST: set temp_fan_on=3500 -> %s\r\n", ok ? "OK" : "FAIL");
                    break;
                case 5U:
                    ok = settings_set_temp_fan_off(2000);
                    serial_printf("SETTINGS TEST: set temp_fan_off=2000 -> %s\r\n", ok ? "OK" : "FAIL");
                    break;
                case 6U:
                    ok = settings_set_temp_critical(7000);
                    serial_printf("SETTINGS TEST: set temp_critical=7000 -> %s\r\n", ok ? "OK" : "FAIL");
                    break;
                case 7U:
                    ok = settings_reset_to_defaults();
                    serial_printf("SETTINGS TEST: reset to defaults -> %s\r\n", ok ? "OK" : "FAIL");
                    break;
                default:
                    break;
            }

            settings_test_print();
            step = (uint8_t)((step + 1U) % 8U);
        }
    }

    fan_control_init(timers_get_fan_power(), timers_get_fan_remote());
    fan_init(25000);
    fan_control_set_unit_duty(1, 50);
    fan_control_set_unit_duty(2, 10);
    fan_control_set_unit_duty(3, 60);
    fan_control_set_unit_duty(4, 80);

    temperature_sensor_init();

    pwm_repeater_init(timers_get_capture(), timers_get_repeater_a(), timers_get_repeater_b());
    pwm_set_throttle_a((uint32_t)s->pwm_throttle_a);
    pwm_set_throttle_b((uint32_t)s->pwm_throttle_b);

    temperature_sensor_set_setpoint_a(s->temp_fan_off);
    temperature_sensor_set_setpoint_b(s->temp_fan_on);
    temperature_sensor_set_hysteresis(50);
    temperature_sensor_register_handler(temperature_sensor_event_handler);

    while (1)
    {
        watchdog_kick();

        usb_task();
        pwm_repeater_task();
        temperature_sensor_task();

        static uint32_t last_init_debug = 0;
        static uint32_t last_fan_test = 0;
        static bool fan_toggle = false;

        if (HAL_GetTick() - last_fan_test >= 2000)
        {
            last_fan_test = HAL_GetTick();
            fan_toggle = !fan_toggle;

            if (fan_toggle)
            {
                fan_control_set_power_channel_duty(FanChannelTwo, 50);
                fan_control_set_remote_channel_duty(FanChannelOne, 0);
                usb_printf("FAN CH1: POWER ON, REMOTE OFF\r\n");
                serial_printf("FAN CH1: POWER ON, REMOTE OFF\r\n");
            }
            else
            {
                fan_control_set_power_channel_duty(FanChannelTwo, 0);
                fan_control_set_remote_channel_duty(FanChannelOne, 50);
                usb_printf("FAN CH1: POWER OFF, REMOTE ON\r\n");
                serial_printf("FAN CH1: POWER OFF, REMOTE ON\r\n");
            }
        }

        if (HAL_GetTick() - last_init_debug >= 1000)
        {
            last_init_debug = HAL_GetTick();

            usb_printf("CH_A: %u Hz, %u\r\n", (unsigned int)pwm_get_frequency_a(),
                (unsigned int)pwm_get_duty_a());
            usb_printf("CH_B: %u Hz, %u\r\n", (unsigned int)pwm_get_frequency_b(),
                (unsigned int)pwm_get_duty_b());

            serial_printf("CH_A: %u Hz, %u\r\n", (unsigned int)pwm_get_frequency_a(),
                (unsigned int)pwm_get_duty_a());
            serial_printf("CH_B: %u Hz, %u\r\n", (unsigned int)pwm_get_frequency_b(),
                (unsigned int)pwm_get_duty_b());

            uint16_t raw_temp = get_temperature();
            if (raw_temp == 0xFFFF)
            {
                usb_printf("TEMP: SENSOR LOST\r\n");
                serial_printf("TEMP: SENSOR LOST\r\n");
            }
            else
            {
                usb_printf("TEMP: %u\r\n", raw_temp);
                serial_printf("TEMP: %u\r\n", raw_temp);
            }
        }
    }
}
