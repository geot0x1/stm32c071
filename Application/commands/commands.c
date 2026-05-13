#include "commands.h"
#include "app_state.h"
#include "settings.h"
#include "telemetry.h"
#include "usb.h"
#include "fan_control.h"
#include "pwm_repeater.h"
#include "fifo.h"
#include "stm32c0xx_hal.h"
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>

#define CMD_LINE_BUF_SIZE 128U
#define CMD_USB_CHUNK_SIZE 64U

#define CMD_TEMPON_MIN 1
#define CMD_TEMPON_MAX 80
#define CMD_TEMPOFF_MIN 0
#define CMD_TEMPOFF_MAX 79
#define CMD_TEMPCRIT_MIN 2
#define CMD_TEMPCRIT_MAX 90

typedef struct
{
    char buf[CMD_LINE_BUF_SIZE];
    uint8_t len;
}CommandLineBuffer;

static CommandLineBuffer lineBuf = {0};

static uint8_t usb_fifo_buffer[256];
static Fifo usb_fifo;

static bool parse_int(const char *s, int32_t *out)
{
    if ((s == NULL) || (*s == '\0'))
    {
        return false;
    }
    *out = (int32_t)atoi(s);
    return true;
}


static bool parse_pwm_throttle(const char *params)
{
    if (*params == '\0')
    {
        usb_printf("ERR INVALID_FORMAT PWMTHR\r\n");
        return false;
    }
    char channel = *params;
    if ((channel != 'A') && (channel != 'B') && (channel != 'a') && (channel != 'b'))
    {
        usb_printf("ERR INVALID_CHANNEL %c\r\n", channel);
        return false;
    }
    if (params[1] != ',')
    {
        usb_printf("ERR INVALID_FORMAT PWMTHR\r\n");
        return false;
    }
    int32_t dc;
    if (!parse_int(params + 2, &dc))
    {
        usb_printf("ERR INVALID_VALUE PWMTHR %s\r\n", params + 2);
        return false;
    }
    if ((dc < 0) || (dc > 100))
    {
        usb_printf("ERR OUT_OF_RANGE PWMTHR %d\r\n", (int)dc);
        return false;
    }
    bool success = ((channel == 'A') || (channel == 'a'))
        ? settings_set_pwm_throttle_a((uint8_t)dc)
        : settings_set_pwm_throttle_b((uint8_t)dc);
    if (!success)
    {
        usb_printf("ERR SAVE_FAILED PWMTHR\r\n");
        return false;
    }
    usb_printf("OK SETTINGSCHANGE PWMTHR %c %d\r\n", channel, (int)dc);
    return true;
}

static bool parse_fan_temp_on(const char *value_str)
{
    int32_t val;
    if (!parse_int(value_str, &val))
    {
        usb_printf("ERR INVALID_VALUE FANTEMPON %s\r\n", value_str);
        return false;
    }
    if ((val < CMD_TEMPON_MIN) || (val > CMD_TEMPON_MAX))
    {
        usb_printf("ERR OUT_OF_RANGE FANTEMPON %d\r\n", (int)val);
        return false;
    }
    const Settings *s = settings_get();
    if (val <= s->temp_fan_off)
    {
        usb_printf("ERR ORDERING FANTEMPON %d must be > FANTEMPOFF %d\r\n", (int)val,
            (int)s->temp_fan_off);
        return false;
    }
    if (val >= s->temp_critical)
    {
        usb_printf("ERR ORDERING FANTEMPON %d must be < TEMPCRIT %d\r\n", (int)val,
            (int)s->temp_critical);
        return false;
    }
    if (!settings_set_temp_fan_on((uint8_t)val))
    {
        usb_printf("ERR SAVE_FAILED FANTEMPON\r\n");
        return false;
    }
    usb_printf("OK SETTINGSCHANGE FANTEMPON %d\r\n", (int)val);
    return true;
}

static bool parse_fan_temp_off(const char *value_str)
{
    int32_t val;
    if (!parse_int(value_str, &val))
    {
        usb_printf("ERR INVALID_VALUE FANTEMPOFF %s\r\n", value_str);
        return false;
    }
    if ((val < CMD_TEMPOFF_MIN) || (val > CMD_TEMPOFF_MAX))
    {
        usb_printf("ERR OUT_OF_RANGE FANTEMPOFF %d\r\n", (int)val);
        return false;
    }
    const Settings *s = settings_get();
    if (val >= s->temp_fan_on)
    {
        usb_printf("ERR ORDERING FANTEMPOFF %d must be < FANTEMPON %d\r\n", (int)val,
            (int)s->temp_fan_on);
        return false;
    }
    if (!settings_set_temp_fan_off((uint8_t)val))
    {
        usb_printf("ERR SAVE_FAILED FANTEMPOFF\r\n");
        return false;
    }
    usb_printf("OK SETTINGSCHANGE FANTEMPOFF %d\r\n", (int)val);
    return true;
}

static bool parse_pwm_throttle_temp(const char *value_str)
{
    int32_t val;
    if (!parse_int(value_str, &val))
    {
        usb_printf("ERR INVALID_VALUE PWMTHRTEMP %s\r\n", value_str);
        return false;
    }
    if (!settings_set_temp_throttle_on((uint8_t)val))
    {
        usb_printf("ERR SAVE_FAILED PWMTHRTEMP\r\n");
        return false;
    }
    usb_printf("OK SETTINGSCHANGE PWMTHRTEMP %d\r\n", (int)val);
    return true;
}

static bool parse_critical_temp(const char *value_str)
{
    int32_t val;
    if (!parse_int(value_str, &val))
    {
        usb_printf("ERR INVALID_VALUE TEMPCRIT %s\r\n", value_str);
        return false;
    }
    if ((val < CMD_TEMPCRIT_MIN) || (val > CMD_TEMPCRIT_MAX))
    {
        usb_printf("ERR OUT_OF_RANGE TEMPCRIT %d\r\n", (int)val);
        return false;
    }
    const Settings *s = settings_get();
    if (val <= s->temp_fan_on)
    {
        usb_printf("ERR ORDERING TEMPCRIT %d must be > FANTEMPON %d\r\n", (int)val,
            (int)s->temp_fan_on);
        return false;
    }
    if (!settings_set_temp_critical((uint8_t)val))
    {
        usb_printf("ERR SAVE_FAILED TEMPCRIT\r\n");
        return false;
    }
    usb_printf("OK SETTINGSCHANGE TEMPCRIT %d\r\n", (int)val);
    return true;
}

// need review if this reset should happen in this module.
static void handle_reset(void)
{
    usb_printf("OK RESET\r\n");
    for (uint8_t i = 0U; i < 10U; i++)
    {
        usb_task();
        HAL_Delay(10U);
    }
    HAL_NVIC_SystemReset();
}

static void handle_fan(const char *token)
{
    const char *equals = strchr(token, '=');
    if (equals == NULL)
    {
        usb_printf("ERR INVALID_FORMAT FAN\r\n");
        return;
    }

    const char *fan_str = token + 3;
    int32_t fan_num;
    if (!parse_int(fan_str, &fan_num) || (fan_num < 1) || (fan_num > 4))
    {
        usb_printf("ERR INVALID_FAN_NUM %s\r\n", fan_str);
        return;
    }

    const char *state_str = equals + 1;
    uint8_t duty;

    if (strcmp(state_str, "ON") == 0)
    {
        duty = 100U;
    }
    else if (strcmp(state_str, "OFF") == 0)
    {
        duty = 0U;
    }
    else
    {
        usb_printf("ERR INVALID_STATE %s\r\n", state_str);
        return;
    }

    fan_control_set_unit_duty((uint8_t)fan_num, duty);
    usb_printf("OK FAN %ld %s\r\n", (long)fan_num, state_str);
}

static void handle_mode(const char *token)
{
    const char *equals = strchr(token, '=');
    if (equals == NULL)
    {
        usb_printf("ERR INVALID_FORMAT mode\r\n");
        return;
    }

    const char *mode_str = equals + 1;

    if (strcmp(mode_str, "NORMAL") == 0)
    {
        app_set_mode(ModeNormal);
        usb_printf("OK MODE NORMAL\r\n");
    }
    else if (strcmp(mode_str, "MANUAL") == 0)
    {
        app_set_mode(ModeManual);
        usb_printf("OK MODE MANUAL\r\n");
    }
    else
    {
        usb_printf("ERR INVALID_MODE %s\r\n", mode_str);
    }
}

static void handle_settings(void)
{
    const Settings *s = settings_get();
    usb_printf("PWM_THROTTLE_A=%u\r\n", s->pwm_throttle_a);
    usb_printf("PWM_THROTTLE_B=%u\r\n", s->pwm_throttle_b);
    usb_printf("TEMP_THROTTLE_ON=%d\r\n", (int)s->temp_throttle_on);
    usb_printf("TEMP_FAN_ON=%d\r\n", (int)s->temp_fan_on);
    usb_printf("TEMP_FAN_OFF=%d\r\n", (int)s->temp_fan_off);
    usb_printf("TEMP_CRITICAL=%d\r\n", (int)s->temp_critical);
}


static bool linebuffer_append_byte(uint8_t byte)
{
    if (lineBuf.len + 2U <= CMD_LINE_BUF_SIZE)
    {
        lineBuf.buf[lineBuf.len++] = (char)byte;
        lineBuf.buf[lineBuf.len] = '\0';
        return true;
    }
    return false;
}

static void linebuffer_clear(void)
{
    lineBuf.len = 0U;
}

static char *usb_read_line(void)
{
    uint8_t chunk[CMD_USB_CHUNK_SIZE];
    uint32_t n = usb_read(chunk, CMD_USB_CHUNK_SIZE);

    if (n > 0)
    {
        fifo_push_array(&usb_fifo, chunk, (uint16_t)n);
    }

    uint8_t byte;
    while (fifo_pop(&usb_fifo, &byte))
    {
        if ((byte == '\r') || (byte == '\n'))
        {
            usb_write("\r\n", 2U);
            if (lineBuf.len > 0U)
            {
                char *result = lineBuf.buf;
                return result;
            }
        }
        else if (byte < 128)
        {
            if (linebuffer_append_byte(byte))
            {
                usb_write(&byte, 1U);
            }
            else
            {
                linebuffer_clear();
                usb_printf("ERR OVERFLOW\r\n");
            }
        }
    }

    return NULL;
}

static void process_line(void)
{
    usb_printf("\r\n");
    usb_printf("[CMD] Received line: %s\r\n", lineBuf.buf);

    /* Query settings: SETTINGS? */
    if (strcmp(lineBuf.buf, "SETTINGS?") == 0)
    {
        handle_settings();
        return;
    }

    /* System reset: RESET */
    if (strcmp(lineBuf.buf, "RESET") == 0)
    {
        handle_reset();
        return;
    }

    /* Application mode: MODE=<NORMAL|MANUAL> */
    if (strstr(lineBuf.buf, "MODE=") == lineBuf.buf)
    {
        handle_mode(lineBuf.buf);
        return;
    }

    /* PWMTHR=<A|B><0-100> - PWM throttle */
    if (strstr(lineBuf.buf, "PWMTHR=") == lineBuf.buf)
    {
        parse_pwm_throttle(lineBuf.buf + 7);
        return;
    }

    /* FANTEMPON=<1-80> - Fan ON temperature (check before FAN control) */
    if (strstr(lineBuf.buf, "FANTEMPON=") == lineBuf.buf)
    {
        parse_fan_temp_on(lineBuf.buf + 10);
        return;
    }

    /* FANTEMPOFF=<0-79> - Fan OFF temperature (check before FAN control) */
    if (strstr(lineBuf.buf, "FANTEMPOFF=") == lineBuf.buf)
    {
        parse_fan_temp_off(lineBuf.buf + 11);
        return;
    }

    /* PWMTHRTEMP=<value> - PWM throttle temperature threshold */
    if (strstr(lineBuf.buf, "PWMTHRTEMP=") == lineBuf.buf)
    {
        parse_pwm_throttle_temp(lineBuf.buf + 11);
        return;
    }

    /* TEMPCRIT=<2-90> - Critical temperature */
    if (strstr(lineBuf.buf, "TEMPCRIT=") == lineBuf.buf)
    {
        parse_critical_temp(lineBuf.buf + 9);
        return;
    }

    /* Fan control: FAN<1-4>=<ON|OFF> (check after temperature commands) */
    if (strstr(lineBuf.buf, "FAN") == lineBuf.buf && strchr(lineBuf.buf, '=') != NULL)
    {
        handle_fan(lineBuf.buf);
        return;
    }

    /* SETDEFAULT - Reset to defaults */
    if (strcmp(lineBuf.buf, "SETDEFAULT") == 0)
    {
        if (!settings_reset_to_defaults())
        {
            usb_printf("ERR SAVE_FAILED SETDEFAULT\r\n");
            return;
        }
        usb_printf("OK SETTINGSCHANGE SETDEFAULT\r\n");
        return;
    }

    /* GETFW - Report firmware version */
    if (strcmp(lineBuf.buf, "GETFW") == 0)
    {
        usb_printf("FWVER=%s\r\n", FIRMWARE_VERSION);
        return;
    }

    usb_printf("ERR UNKNOWN_CMD\r\n");
}

void commands_init(void)
{
    linebuffer_clear();
    fifo_init(&usb_fifo, usb_fifo_buffer, sizeof(usb_fifo_buffer));
}

void commands_task(void)
{
    char *line = usb_read_line();
    if (line != NULL)
    {
        process_line();
        linebuffer_clear();
    }
}
