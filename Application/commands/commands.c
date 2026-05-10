#include "commands.h"
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

typedef enum
{
    ModeNormal,
    ModeManual,
} AppMode;

extern void app_set_mode(AppMode mode);
extern void app_set_throttle_override(uint32_t throttle_a, uint32_t throttle_b);
extern void app_clear_throttle_override(void);
extern uint32_t app_get_throttle_override_a(void);
extern uint32_t app_get_throttle_override_b(void);

#define CMD_LINE_BUF_SIZE 128U
#define CMD_USB_CHUNK_SIZE 64U
#define CMD_MAX_TOKENS 8U

#define CMD_TEMPON_MIN 1
#define CMD_TEMPON_MAX 80
#define CMD_TEMPOFF_MIN 0
#define CMD_TEMPOFF_MAX 79
#define CMD_TEMPCRIT_MIN 2
#define CMD_TEMPCRIT_MAX 90

typedef enum
{
    SubCmdUnknown = 0,
    SubCmdPwmA,
    SubCmdPwmB,
    SubCmdTempOn,
    SubCmdTempOff,
    SubCmdTempCrit,
    SubCmdDefault
} CommandSubType;

typedef struct
{
    char buf[CMD_LINE_BUF_SIZE];
    uint8_t len;
}CommandLineBuffer;

typedef struct
{
    const char* key;
    void (*handler)(int value);
} CommandEntry;

// static CommandEntry command_table[] = {
//     {"pwma", handle_pwm_a},
//     {"pwmb", handle_pwm_b},
//     {"fantype", handle_fan_type},
//     {"tempon", handle_temp_on},
//     {"tempoff", handle_temp_off},
//     {"tempcrit", handle_temp_crit},
//     {NULL, NULL}
// };

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

static char *strip_spaces(char *s)
{
    while (*s == ' ')
    {
        s++;
    }
    if (*s != '\0')
    {
        char *end = s + strlen(s) - 1;
        while ((end > s) && (*end == ' '))
        {
            *end = '\0';
            end--;
        }
    }
    return s;
}



static CommandSubType identify_sub_cmd(const char *s)
{
    if (strcmp(s, "pwma") == 0)
    {
        return SubCmdPwmA;
    }
    if (strcmp(s, "pwmb") == 0)
    {
        return SubCmdPwmB;
    }
    if (strcmp(s, "tempon") == 0)
    {
        return SubCmdTempOn;
    }
    if (strcmp(s, "tempoff") == 0)
    {
        return SubCmdTempOff;
    }
    if (strcmp(s, "tempcrit") == 0)
    {
        return SubCmdTempCrit;
    }
    if (strcmp(s, "default") == 0)
    {
        return SubCmdDefault;
    }
    return SubCmdUnknown;
}

static void handle_pwm_a(char **tokens, uint8_t count)
{
    if (count != 3U)
    {
        usb_printf("ERR WRONG_ARG_COUNT pwma needs 1 value\r\n");
        return;
    }
    int32_t val;
    if (!parse_int(tokens[2], &val))
    {
        usb_printf("ERR INVALID_VALUE pwma %s\r\n", tokens[2]);
        return;
    }
    if ((val < 0) || (val > 100))
    {
        usb_printf("ERR OUT_OF_RANGE pwma %d\r\n", (int)val);
        return;
    }
    if (!settings_set_pwm_throttle_a((uint8_t)val))
    {
        usb_printf("ERR SAVE_FAILED pwma\r\n");
        return;
    }
    usb_printf("OK SETTINGSCHANGE pwma %d\r\n", (int)val);
}

static void handle_pwm_b(char **tokens, uint8_t count)
{
    if (count != 3U)
    {
        usb_printf("ERR WRONG_ARG_COUNT pwmb needs 1 value\r\n");
        return;
    }
    int32_t val;
    if (!parse_int(tokens[2], &val))
    {
        usb_printf("ERR INVALID_VALUE pwmb %s\r\n", tokens[2]);
        return;
    }
    if ((val < 0) || (val > 100))
    {
        usb_printf("ERR OUT_OF_RANGE pwmb %d\r\n", (int)val);
        return;
    }
    if (!settings_set_pwm_throttle_b((uint8_t)val))
    {
        usb_printf("ERR SAVE_FAILED pwmb\r\n");
        return;
    }
    usb_printf("OK SETTINGSCHANGE pwmb %d\r\n", (int)val);
}

static void handle_temp_on(char **tokens, uint8_t count)
{
    if (count != 3U)
    {
        usb_printf("ERR WRONG_ARG_COUNT tempon needs 1 value\r\n");
        return;
    }
    int32_t val;
    if (!parse_int(tokens[2], &val))
    {
        usb_printf("ERR INVALID_VALUE tempon %s\r\n", tokens[2]);
        return;
    }
    if ((val < CMD_TEMPON_MIN) || (val > CMD_TEMPON_MAX))
    {
        usb_printf("ERR OUT_OF_RANGE tempon %d\r\n", (int)val);
        return;
    }
    int16_t centideg = (int16_t)(val * 100);
    const Settings *s = settings_get();
    if (centideg <= s->temp_fan_off)
    {
        usb_printf("ERR ORDERING tempon %d must be > tempoff %d\r\n", (int)val,
            (int)(s->temp_fan_off / 100));
        return;
    }
    if (centideg >= s->temp_critical)
    {
        usb_printf("ERR ORDERING tempon %d must be < tempcrit %d\r\n", (int)val,
            (int)(s->temp_critical / 100));
        return;
    }
    if (!settings_set_temp_fan_on(centideg))
    {
        usb_printf("ERR SAVE_FAILED tempon\r\n");
        return;
    }
    usb_printf("OK SETTINGSCHANGE tempon %d\r\n", (int)val);
}

static void handle_temp_off(char **tokens, uint8_t count)
{
    if (count != 3U)
    {
        usb_printf("ERR WRONG_ARG_COUNT tempoff needs 1 value\r\n");
        return;
    }
    int32_t val;
    if (!parse_int(tokens[2], &val))
    {
        usb_printf("ERR INVALID_VALUE tempoff %s\r\n", tokens[2]);
        return;
    }
    if ((val < CMD_TEMPOFF_MIN) || (val > CMD_TEMPOFF_MAX))
    {
        usb_printf("ERR OUT_OF_RANGE tempoff %d\r\n", (int)val);
        return;
    }
    int16_t centideg = (int16_t)(val * 100);
    const Settings *s = settings_get();
    if (centideg >= s->temp_fan_on)
    {
        usb_printf("ERR ORDERING tempoff %d must be < tempon %d\r\n", (int)val,
            (int)(s->temp_fan_on / 100));
        return;
    }
    if (!settings_set_temp_fan_off(centideg))
    {
        usb_printf("ERR SAVE_FAILED tempoff\r\n");
        return;
    }
    usb_printf("OK SETTINGSCHANGE tempoff %d\r\n", (int)val);
}

static void handle_temp_crit(char **tokens, uint8_t count)
{
    if (count != 3U)
    {
        usb_printf("ERR WRONG_ARG_COUNT tempcrit needs 1 value\r\n");
        return;
    }
    int32_t val;
    if (!parse_int(tokens[2], &val))
    {
        usb_printf("ERR INVALID_VALUE tempcrit %s\r\n", tokens[2]);
        return;
    }
    if ((val < CMD_TEMPCRIT_MIN) || (val > CMD_TEMPCRIT_MAX))
    {
        usb_printf("ERR OUT_OF_RANGE tempcrit %d\r\n", (int)val);
        return;
    }
    int16_t centideg = (int16_t)(val * 100);
    const Settings *s = settings_get();
    if (centideg <= s->temp_fan_on)
    {
        usb_printf("ERR ORDERING tempcrit %d must be > tempon %d\r\n", (int)val,
            (int)(s->temp_fan_on / 100));
        return;
    }
    if (!settings_set_temp_critical(centideg))
    {
        usb_printf("ERR SAVE_FAILED tempcrit\r\n");
        return;
    }
    usb_printf("OK SETTINGSCHANGE tempcrit %d\r\n", (int)val);
}

static void handle_default(uint8_t count)
{
    if (count != 2U)
    {
        usb_printf("ERR WRONG_ARG_COUNT default takes no arguments\r\n");
        return;
    }
    if (!settings_reset_to_defaults())
    {
        usb_printf("ERR SAVE_FAILED default\r\n");
        return;
    }
    usb_printf("OK SETTINGSCHANGE default\r\n");
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

static void handle_telemetry(char **tokens, uint8_t count)
{
    if (count < 2U)
    {
        usb_printf("ERR WRONG_ARG_COUNT TELEMETRY\r\n");
        return;
    }

    if (strcmp(tokens[1], "ON") == 0)
    {
        if (count != 2U)
        {
            usb_printf("ERR WRONG_ARG_COUNT TELEMETRY ON takes no arguments\r\n");
            return;
        }
        telemetry_enable(true);
        usb_printf("OK TELEMETRY ON\r\n");
    }
    else if (strcmp(tokens[1], "OFF") == 0)
    {
        if (count != 2U)
        {
            usb_printf("ERR WRONG_ARG_COUNT TELEMETRY OFF takes no arguments\r\n");
            return;
        }
        telemetry_enable(false);
        usb_printf("OK TELEMETRY OFF\r\n");
    }
    else if (strcmp(tokens[1], "INTERVAL") == 0)
    {
        if (count != 3U)
        {
            usb_printf("ERR WRONG_ARG_COUNT TELEMETRY INTERVAL needs a value\r\n");
            return;
        }
        int32_t val;
        if (!parse_int(tokens[2], &val))
        {
            usb_printf("ERR INVALID_VALUE TELEMETRY INTERVAL %s\r\n", tokens[2]);
            return;
        }
        if (val < (int32_t)TELEMETRY_MIN_INTERVAL_MS || val > (int32_t)TELEMETRY_MAX_INTERVAL_MS)
        {
            usb_printf("ERR OUT_OF_RANGE TELEMETRY INTERVAL %ld (min %lu max %lu)\r\n", (long)val,
                (unsigned long)TELEMETRY_MIN_INTERVAL_MS, (unsigned long)TELEMETRY_MAX_INTERVAL_MS);
            return;
        }
        telemetry_set_interval_ms((uint32_t)val);
        usb_printf("OK TELEMETRY INTERVAL %lu\r\n", (unsigned long)val);
    }
    else
    {
        usb_printf("ERR UNKNOWN_SUBCMD TELEMETRY %s\r\n", tokens[1]);
    }
}

static void handle_fan(const char *token)
{
    const char *equals = strchr(token, '=');
    if (equals == NULL)
    {
        usb_printf("ERR INVALID_FORMAT fan\r\n");
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

static void handle_pwm_throttle(const char *token)
{
    const char *equals = strchr(token, '=');
    if (equals == NULL)
    {
        usb_printf("ERR INVALID_FORMAT pwmthr\r\n");
        return;
    }

    const char *params = equals + 1;
    const char *comma = strchr(params, ',');
    if (comma == NULL)
    {
        usb_printf("ERR INVALID_FORMAT pwmthr\r\n");
        return;
    }

    char channel = *params;
    if ((channel != 'A') && (channel != 'B') && (channel != 'a') && (channel != 'b'))
    {
        usb_printf("ERR INVALID_CHANNEL %c\r\n", channel);
        return;
    }

    int32_t throttle;
    if (!parse_int(comma + 1, &throttle))
    {
        usb_printf("ERR INVALID_VALUE pwmthr %s\r\n", comma + 1);
        return;
    }

    if ((throttle < 0) || (throttle > 100))
    {
        usb_printf("ERR OUT_OF_RANGE pwmthr %d\r\n", (int)throttle);
        return;
    }

    uint32_t new_throttle_a = app_get_throttle_override_a();
    uint32_t new_throttle_b = app_get_throttle_override_b();

    if ((channel == 'A') || (channel == 'a'))
    {
        new_throttle_a = (uint32_t)throttle;
        usb_printf("OK PWMTHR A %d\r\n", (int)throttle);
    }
    else
    {
        new_throttle_b = (uint32_t)throttle;
        usb_printf("OK PWMTHR B %d\r\n", (int)throttle);
    }

    app_set_throttle_override(new_throttle_a, new_throttle_b);
}

static bool linebuffer_append_byte(uint8_t byte)
{
    if (lineBuf.len < (CMD_LINE_BUF_SIZE - 1U))
    {
        lineBuf.buf[lineBuf.len++] = (char)byte;
        return true;
    }
    return false;
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
                lineBuf.buf[lineBuf.len] = '\0';
                char *result = lineBuf.buf;
                lineBuf.len = 0U;
                return result;
            }
        }
        else if ((byte == 0x08U) || (byte == 0x7FU))
        {
            if (lineBuf.len > 0U)
            {
                lineBuf.len--;
                usb_write("\b \b", 3U);
            }
        }
        else if ((byte >= 0x20U) && (byte <= 0x7EU))
        {
            if (linebuffer_append_byte(byte))
            {
                usb_write(&byte, 1U);
            }
            else
            {
                lineBuf.len = 0U;
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

    if (strstr(lineBuf.buf, "PWMTHR=") == lineBuf.buf && strchr(lineBuf.buf, ',') != NULL)
    {
        handle_pwm_throttle(lineBuf.buf);
        return;
    }

    if (strstr(lineBuf.buf, "FAN") == lineBuf.buf && strchr(lineBuf.buf, '=') != NULL)
    {
        handle_fan(lineBuf.buf);
        return;
    }

    if (strstr(lineBuf.buf, "MODE=") == lineBuf.buf)
    {
        handle_mode(lineBuf.buf);
        return;
    }
}

void commands_init(void)
{
    lineBuf.len = 0U;
    fifo_init(&usb_fifo, usb_fifo_buffer, sizeof(usb_fifo_buffer));
}

void commands_task(void)
{
    char *line = usb_read_line();
    if (line != NULL)
    {
        process_line();
    }
}
