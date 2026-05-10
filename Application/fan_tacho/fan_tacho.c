#include "fan_tacho.h"
#include "gpio.h"
#include "board_config.h"
#include "fan_control.h"
#include "stm32c0xx_hal.h"

#define FAN_TACHO_PRESENCE_SAMPLE_MS           100U
#define FAN_TACHO_RPM_PRESENT_THRESHOLD        100U
#define FAN_TACHO_PRESENCE_MISSING_DEBOUNCE_MS 1500U

typedef struct
{
    Gpio gpio;
    volatile uint32_t last_pulse_ms;
    volatile uint32_t prev_pulse_ms;
    volatile bool valid;
    bool enabled;
} TachoChannel;

typedef struct
{
    bool present;
    uint32_t missing_since_ms;
} PresenceState;

static TachoChannel tacho[FAN_TACHO_COUNT];
static PresenceState presence[FAN_TACHO_COUNT];
static uint32_t last_presence_sample_ms;

static GPIO_TypeDef *const ports[FAN_TACHO_COUNT] = {
    BOARD_FAN1_TACHO_PORT,
    BOARD_FAN2_TACHO_PORT,
    BOARD_FAN3_TACHO_PORT,
    BOARD_FAN4_TACHO_PORT,
};

static const uint16_t pins[FAN_TACHO_COUNT] = {
    BOARD_FAN1_TACHO_PIN,
    BOARD_FAN2_TACHO_PIN,
    BOARD_FAN3_TACHO_PIN,
    BOARD_FAN4_TACHO_PIN,
};

void fan_tacho_init(uint8_t fan_idx)
{
    if (fan_idx < 1 || fan_idx > FAN_TACHO_COUNT)
    {
        return;
    }
    uint8_t idx = fan_idx - 1;
    tacho[idx].gpio.port = ports[idx];
    tacho[idx].gpio.pin = pins[idx];
    tacho[idx].last_pulse_ms = 0;
    tacho[idx].prev_pulse_ms = 0;
    tacho[idx].valid = false;
    tacho[idx].enabled = false;
}

void fan_tacho_enable(uint8_t fan_idx)
{
    if (fan_idx < 1 || fan_idx > FAN_TACHO_COUNT)
    {
        return;
    }
    uint8_t idx = fan_idx - 1;

    gpio_exti_init(&tacho[idx].gpio, ports[idx], pins[idx], GPIO_MODE_IT_FALLING, GPIO_NOPULL,
        EXTI4_15_IRQn, 1);

    tacho[idx].last_pulse_ms = 0;
    tacho[idx].prev_pulse_ms = 0;
    tacho[idx].valid = false;
    tacho[idx].enabled = true;
}

void fan_tacho_disable(uint8_t fan_idx)
{
    if (fan_idx < 1 || fan_idx > FAN_TACHO_COUNT)
    {
        return;
    }
    uint8_t idx = fan_idx - 1;

    gpio_input_init(&tacho[idx].gpio, ports[idx], pins[idx], GPIO_NOPULL);

    tacho[idx].enabled = false;
    tacho[idx].valid = false;
}

uint32_t fan_tacho_get_rpm(uint8_t fan_idx)
{
    if (fan_idx < 1 || fan_idx > FAN_TACHO_COUNT)
    {
        return 0;
    }
    uint8_t idx = fan_idx - 1;

    if (!tacho[idx].enabled || !tacho[idx].valid)
    {
        return 0;
    }

    uint32_t pri = __get_PRIMASK();
    __disable_irq();
    uint32_t last = tacho[idx].last_pulse_ms;
    uint32_t prev = tacho[idx].prev_pulse_ms;
    if (!pri)
    {
        __enable_irq();
    }

    if ((HAL_GetTick() - last) > 1000U)
    {
        return 0;
    }

    uint32_t period_ms = last - prev;
    if (period_ms == 0)
    {
        return 0;
    }

    /* 30000 = 60000 ms/min / 2 pulses-per-rev */
    return 30000U / period_ms;
}

void HAL_GPIO_EXTI_Falling_Callback(uint16_t GPIO_Pin)
{
    uint32_t now = HAL_GetTick();

    for (uint8_t i = 0; i < FAN_TACHO_COUNT; i++)
    {
        if (tacho[i].enabled && tacho[i].gpio.pin == GPIO_Pin)
        {
            tacho[i].prev_pulse_ms = tacho[i].last_pulse_ms;
            tacho[i].last_pulse_ms = now;
            tacho[i].valid = true;
            break;
        }
    }
}

void fan_tacho_update_presence(uint32_t now_ms)
{
    if (now_ms - last_presence_sample_ms < FAN_TACHO_PRESENCE_SAMPLE_MS)
    {
        return;
    }
    last_presence_sample_ms = now_ms;

    for (uint8_t i = 0U; i < FAN_TACHO_COUNT; i++)
    {
        uint8_t unit = i + 1U;

        if (fan_control_get_type(unit) == FanType2Wire)
        {
            presence[i].present = true;
            presence[i].missing_since_ms = 0U;
            continue;
        }

        bool commanded_on = (fan_control_get_unit_duty(unit) > 0U);
        if (!commanded_on)
        {
            presence[i].missing_since_ms = 0U;
            continue;
        }

        uint32_t rpm = fan_tacho_get_rpm(unit);
        if (rpm >= FAN_TACHO_RPM_PRESENT_THRESHOLD)
        {
            presence[i].present = true;
            presence[i].missing_since_ms = 0U;
        }
        else
        {
            if (presence[i].missing_since_ms == 0U)
            {
                presence[i].missing_since_ms = now_ms;
            }
            else if (now_ms - presence[i].missing_since_ms >= FAN_TACHO_PRESENCE_MISSING_DEBOUNCE_MS)
            {
                presence[i].present = false;
            }
        }
    }
}

bool fan_tacho_is_present(uint8_t fan_idx)
{
    if (fan_idx < 1 || fan_idx > FAN_TACHO_COUNT)
    {
        return false;
    }
    return presence[fan_idx - 1U].present;
}
