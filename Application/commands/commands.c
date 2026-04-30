#include "commands.h"
#include "settings.h"
#include "usb.h"
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#define CMD_LINE_BUF_SIZE    128U
#define CMD_USB_CHUNK_SIZE    64U
#define CMD_MAX_TOKENS         8U

#define CMD_TEMPON_MIN    1
#define CMD_TEMPON_MAX   80
#define CMD_TEMPOFF_MIN   0
#define CMD_TEMPOFF_MAX  79
#define CMD_TEMPCRIT_MIN  2
#define CMD_TEMPCRIT_MAX 90

typedef enum
{
    SubCmdUnknown  = 0,
    SubCmdPwmA,
    SubCmdPwmB,
    SubCmdFanType,
    SubCmdTempOn,
    SubCmdTempOff,
    SubCmdTempCrit
} CommandSubType;

static char    lineBuf[CMD_LINE_BUF_SIZE];
static uint8_t lineLen;

static bool parse_int(const char *s, int32_t *out)
{
    if ((s == NULL) || (*s == '\0'))
    {
        return false;
    }

    bool negative = false;
    if (*s == '-')
    {
        negative = true;
        s++;
    }

    if (*s == '\0')
    {
        return false;
    }

    int32_t acc = 0;
    while (*s != '\0')
    {
        if ((*s < '0') || (*s > '9'))
        {
            return false;
        }
        int32_t digit = (int32_t)(*s - '0');
        if (acc > (INT32_MAX - digit) / 10)
        {
            return false;
        }
        acc = (acc * 10) + digit;
        s++;
    }

    *out = negative ? -acc : acc;
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

static uint8_t tokenize(char *buf, char **tokens, uint8_t max)
{
    uint8_t count = 0;
    char *p = buf;

    while ((*p != '\0') && (count < max))
    {
        char *comma = strchr(p, ',');
        if (comma != NULL)
        {
            *comma = '\0';
        }
        tokens[count] = strip_spaces(p);
        count++;
        if (comma == NULL)
        {
            break;
        }
        p = comma + 1;
    }

    return count;
}

static CommandSubType identify_sub_cmd(const char *s)
{
    if (strcmp(s, "pwma")     == 0) { return SubCmdPwmA; }
    if (strcmp(s, "pwmb")     == 0) { return SubCmdPwmB; }
    if (strcmp(s, "fantype")  == 0) { return SubCmdFanType; }
    if (strcmp(s, "tempon")   == 0) { return SubCmdTempOn; }
    if (strcmp(s, "tempoff")  == 0) { return SubCmdTempOff; }
    if (strcmp(s, "tempcrit") == 0) { return SubCmdTempCrit; }
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

static void handle_fan_type(char **tokens, uint8_t count)
{
    if (count != 6U)
    {
        usb_printf("ERR WRONG_ARG_COUNT fantype needs 4 values\r\n");
        return;
    }
    int32_t vals[4];
    for (uint8_t i = 0U; i < 4U; i++)
    {
        if (!parse_int(tokens[2U + i], &vals[i]))
        {
            usb_printf("ERR INVALID_VALUE fantype[%u] %s\r\n", (unsigned int)i, tokens[2U + i]);
            return;
        }
        if ((vals[i] < 0) || (vals[i] > 2))
        {
            usb_printf("ERR OUT_OF_RANGE fantype[%u] %d\r\n", (unsigned int)i, (int)vals[i]);
            return;
        }
    }
    for (uint8_t i = 0U; i < 4U; i++)
    {
        if (!settings_set_fan_type_override(i, (FanTypeOverride)vals[i]))
        {
            usb_printf("ERR SAVE_FAILED fantype[%u]\r\n", (unsigned int)i);
            return;
        }
    }
    usb_printf("OK SETTINGSCHANGE fantype %d %d %d %d\r\n",
        (int)vals[0], (int)vals[1], (int)vals[2], (int)vals[3]);
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
        usb_printf("ERR ORDERING tempon %d must be > tempoff %d\r\n",
            (int)val, (int)(s->temp_fan_off / 100));
        return;
    }
    if (centideg >= s->temp_critical)
    {
        usb_printf("ERR ORDERING tempon %d must be < tempcrit %d\r\n",
            (int)val, (int)(s->temp_critical / 100));
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
        usb_printf("ERR ORDERING tempoff %d must be < tempon %d\r\n",
            (int)val, (int)(s->temp_fan_on / 100));
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
        usb_printf("ERR ORDERING tempcrit %d must be > tempon %d\r\n",
            (int)val, (int)(s->temp_fan_on / 100));
        return;
    }
    if (!settings_set_temp_critical(centideg))
    {
        usb_printf("ERR SAVE_FAILED tempcrit\r\n");
        return;
    }
    usb_printf("OK SETTINGSCHANGE tempcrit %d\r\n", (int)val);
}

static void process_line(void)
{
    lineBuf[lineLen] = '\0';

    char *tokens[CMD_MAX_TOKENS];
    uint8_t count = tokenize(lineBuf, tokens, CMD_MAX_TOKENS);

    if (count == 0U)
    {
        return;
    }

    if (strcmp(tokens[0], "SETTINGSCHANGE") != 0)
    {
        usb_printf("ERR UNKNOWN_CMD %s\r\n", tokens[0]);
        return;
    }

    if (count < 2U)
    {
        usb_printf("ERR WRONG_ARG_COUNT SETTINGSCHANGE needs a sub-command\r\n");
        return;
    }

    CommandSubType sub = identify_sub_cmd(tokens[1]);
    switch (sub)
    {
        case SubCmdPwmA:    handle_pwm_a(tokens, count);    break;
        case SubCmdPwmB:    handle_pwm_b(tokens, count);    break;
        case SubCmdFanType: handle_fan_type(tokens, count); break;
        case SubCmdTempOn:  handle_temp_on(tokens, count);  break;
        case SubCmdTempOff: handle_temp_off(tokens, count); break;
        case SubCmdTempCrit: handle_temp_crit(tokens, count); break;
        default:
            usb_printf("ERR UNKNOWN_SUBCMD %s\r\n", tokens[1]);
            break;
    }
}

void commands_init(void)
{
    lineLen = 0U;
}

void commands_task(void)
{
    uint8_t chunk[CMD_USB_CHUNK_SIZE];
    uint32_t n = usb_read(chunk, CMD_USB_CHUNK_SIZE);

    for (uint32_t i = 0U; i < n; i++)
    {
        uint8_t b = chunk[i];

        if ((b == '\r') || (b == '\n'))
        {
            if (lineLen > 0U)
            {
                process_line();
                lineLen = 0U;
            }
        }
        else if ((b >= 0x20U) && (b <= 0x7EU))
        {
            if (lineLen < (CMD_LINE_BUF_SIZE - 1U))
            {
                lineBuf[lineLen++] = (char)b;
            }
            else
            {
                lineLen = 0U;
                usb_printf("ERR OVERFLOW\r\n");
            }
        }
    }
}
