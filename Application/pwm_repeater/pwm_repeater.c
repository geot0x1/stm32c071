/**
 * @file pwm_repeater.c
 * @brief TIM2-based PWM Repeater with duty cycle capping.
 */

#include "pwm_repeater.h"
#include "stm32c0xx_hal.h"
#include <stdint.h>

/* ── Tuning constants ──────────────────────────────────────────────────────── */
#define TIMEOUT_TICKS 48000000U /* 1 second at 48 MHz */
#define MAX_DUTY_PCT 100U
#define IC_FILTER_VAL 0x04U
#define MIN_LEVEL_TICKS 100U
#define MAX_LEVEL_TICKS 1200000U
#define MIN_PERIOD_TICKS 1000U
#define MAX_PERIOD_TICKS 1500000U
#define ARR_UPDATE_THRESHOLD 100U
#define STABILITY_THRESHOLD 100U
#define STABILITY_REQUIRED_COUNT 3U

/* ── Module-private timer handles (set by pwm_repeater_init) ───────────────── */
static Tim *_capture_tim = NULL;
static Tim *_out_a_tim = NULL;
static Tim *_out_b_tim = NULL;

/* ── Global instances ──────────────────────────────────────────────────────── */
PwmChannel pwmChannelA = {.rise_captured = false};
PwmChannel pwmChannelB = {.rise_captured = false};
PwmOutput pwmOutputA = {.tim = NULL,
    .channel = TIM_CHANNEL_1,
    .cap_factor_pct = 100,
    .throttle_val = 100,
    .throttle_mode = ThrottleModeScale};
PwmOutput pwmOutputB = {.tim = NULL,
    .channel = TIM_CHANNEL_1,
    .cap_factor_pct = 100,
    .throttle_val = 100,
    .throttle_mode = ThrottleModeScale};

/* ── Forward declarations ──────────────────────────────────────────────────── */
static void handle_ic_capture(
    PwmChannel *ch, uint32_t captured, uint32_t channel, GPIO_TypeDef *port, uint16_t pin);
static void reset_channel_state(PwmChannel *ch, uint32_t channel);
static uint32_t calculate_output_pulse(PwmOutput *out);
static void init_channel_struct(PwmChannel *ch);
static void apply_output_to_hardware(PwmOutput *out, uint32_t active_pulse);
static void process_channel_update(
    PwmChannel *ch, PwmOutput *out, GPIO_TypeDef *port, uint16_t pin);
static uint32_t calculate_frequency(uint32_t period_ticks);
static uint32_t calculate_duty_pct(uint32_t period_ticks, uint32_t pulse_ticks);

/* ── Public API ────────────────────────────────────────────────────────────── */

void pwm_set_throttle_a(uint32_t val, ThrottleMode mode)
{
    if (val > 100)
    {
        val = 100;
    }
    pwmOutputA.throttle_val = val;
    pwmOutputA.throttle_mode = mode;
}

void pwm_set_throttle_b(uint32_t val, ThrottleMode mode)
{
    if (val > 100)
    {
        val = 100;
    }
    pwmOutputB.throttle_val = val;
    pwmOutputB.throttle_mode = mode;
}

void pwm_repeater_init(Tim *capture_tim, Tim *out_a_tim, Tim *out_b_tim)
{
    _capture_tim = capture_tim;
    _out_a_tim = out_a_tim;
    _out_b_tim = out_b_tim;

    pwmOutputA.tim = out_a_tim;
    pwmOutputB.tim = out_b_tim;

    TIM_IC_InitTypeDef sConfigIC = {0};

    /* 1. Configure input capture filters */
    sConfigIC.ICPolarity = TIM_ICPOLARITY_BOTHEDGE;
    sConfigIC.ICSelection = TIM_ICSELECTION_DIRECTTI;
    sConfigIC.ICPrescaler = TIM_ICPSC_DIV1;
    sConfigIC.ICFilter = IC_FILTER_VAL;

    HAL_TIM_IC_ConfigChannel(&_capture_tim->hal_handle, &sConfigIC, TIM_CHANNEL_3);
    HAL_TIM_IC_ConfigChannel(&_capture_tim->hal_handle, &sConfigIC, TIM_CHANNEL_4);

    /* 2. Enable ARR preload on capture timer and both output timers */
    _capture_tim->hal_handle.Instance->CR1 |= TIM_CR1_ARPE;

    _out_a_tim->hal_handle.Instance->CR1 |= TIM_CR1_ARPE;
    _out_a_tim->hal_handle.Instance->PSC = 47; /* 48 MHz → 1 MHz (1 µs resolution) */
    __HAL_TIM_ENABLE_OCxPRELOAD(&_out_a_tim->hal_handle, TIM_CHANNEL_1);

    _out_b_tim->hal_handle.Instance->CR1 |= TIM_CR1_ARPE;
    _out_b_tim->hal_handle.Instance->PSC = 47;
    __HAL_TIM_ENABLE_OCxPRELOAD(&_out_b_tim->hal_handle, TIM_CHANNEL_1);

    /* 3. Reset channel state */
    init_channel_struct(&pwmChannelA);
    init_channel_struct(&pwmChannelB);

    /* 4. Force ARR update and start peripherals */
    _capture_tim->hal_handle.Instance->ARR = UINT32_MAX;
    _capture_tim->hal_handle.Instance->EGR = TIM_EGR_UG;

    HAL_TIM_PWM_Start(&_out_a_tim->hal_handle, TIM_CHANNEL_1);
    HAL_TIM_PWM_Start(&_out_b_tim->hal_handle, TIM_CHANNEL_1);
    HAL_TIM_IC_Start_IT(&_capture_tim->hal_handle, TIM_CHANNEL_3);
    HAL_TIM_IC_Start_IT(&_capture_tim->hal_handle, TIM_CHANNEL_4);

    pwm_set_throttle_a(50, ThrottleModeFixed); /* Default: 50% fixed limit */
    pwm_set_throttle_b(100, ThrottleModeScale); /* Default: pass-through */
}

void pwm_repeater_tim2_irq_handler(void)
{
    HAL_TIM_IRQHandler(&_capture_tim->hal_handle);
}

/* ── Static helpers ────────────────────────────────────────────────────────── */

static uint32_t calculate_delta(uint32_t current, uint32_t previous, uint32_t arr)
{
    if (current >= previous)
    {
        return current - previous;
    }

    return (arr - previous) + current + 1;
}

static uint32_t calculate_output_pulse(PwmOutput *out)
{
    uint32_t active_pulse;

    if (out->throttle_mode == ThrottleModeScale)
    {
        active_pulse = (out->pulse_ticks * out->throttle_val) / 100U;
    }
    else
    {
        active_pulse = (out->period_ticks * out->throttle_val) / 100U;
    }

    if (active_pulse > out->pulse_ticks)
    {
        active_pulse = out->pulse_ticks;
    }

    uint32_t cap_limit = (out->period_ticks * out->cap_factor_pct) / 100U;
    if (active_pulse > cap_limit)
    {
        active_pulse = cap_limit;
    }

    return active_pulse;
}

static void init_channel_struct(PwmChannel *ch)
{
    ch->rise_captured = false;
    ch->fall_captured = false;
    ch->period_ticks = 0;
    ch->pulse_ticks = 0;
    ch->low_level_ticks = 0;
    ch->last_capture_ms = HAL_GetTick();
    ch->new_data_ready = false;
    ch->period_stable_counter = 0;
    ch->previous_period_ticks = 0;
    ch->pulse_stable_counter = 0;
    ch->previous_pulse_ticks = 0;
}

static void apply_output_to_hardware(PwmOutput *out, uint32_t active_pulse)
{
    uint32_t output_period = (out->period_ticks / 48U);
    uint32_t target_arr =
        (output_period > 0xFFFFU) ? 0xFFFFU : (output_period > 0 ? output_period - 1 : 0);
    uint32_t target_ccr = (active_pulse / 48U);

    if (target_ccr > target_arr)
    {
        if (target_arr == 0xFFFFU)
        {
            target_arr = 0xFFFEU;
        }
        target_ccr = target_arr + 1;
    }

    out->tim->hal_handle.Instance->ARR = target_arr;
    out->tim->hal_handle.Instance->CCR1 = target_ccr;
}

static void process_channel_update(PwmChannel *ch, PwmOutput *out, GPIO_TypeDef *port, uint16_t pin)
{
    uint32_t now = HAL_GetTick();
    const uint32_t TIMEOUT_MS = 50;

    if (ch->new_data_ready)
    {
        out->period_ticks = ch->period_ticks;
        out->pulse_ticks = ch->pulse_ticks;

        uint32_t active_pulse = calculate_output_pulse(out);
        apply_output_to_hardware(out, active_pulse);

        ch->new_data_ready = false;
    }

    if ((now - ch->last_capture_ms) > TIMEOUT_MS)
    {
        if (HAL_GPIO_ReadPin(port, pin) == GPIO_PIN_SET)
        {
            if (ch->period_ticks == 0)
            {
                out->tim->hal_handle.Instance->ARR = 0xFFFE;
                out->tim->hal_handle.Instance->CCR1 = 0xFFFF;
            }
            else
            {
                ch->pulse_ticks = ch->period_ticks;
                out->period_ticks = ch->period_ticks;
                out->pulse_ticks = ch->pulse_ticks;

                uint32_t active_pulse = calculate_output_pulse(out);
                apply_output_to_hardware(out, active_pulse);
            }
        }
        else
        {
            out->tim->hal_handle.Instance->CCR1 = 0;
            init_channel_struct(ch);
        }
    }
}

static void reset_channel_state(PwmChannel *ch, uint32_t channel)
{
    (void)channel;
    ch->rise_captured = false;
    ch->fall_captured = false;
    ch->new_data_ready = false;
    ch->period_ticks = 0;
    ch->pulse_ticks = 0;
    ch->low_level_ticks = 0;
    ch->period_stable_counter = 0;
    ch->previous_period_ticks = 0;
    ch->pulse_stable_counter = 0;
    ch->previous_pulse_ticks = 0;
}

static uint32_t calculate_frequency(uint32_t period_ticks)
{
    if (period_ticks == 0)
    {
        return 0;
    }

    return 1000000U / period_ticks;
}

static uint32_t calculate_duty_pct(uint32_t period_ticks, uint32_t pulse_ticks)
{
    if (period_ticks == 0)
    {
        return 0;
    }

    return (pulse_ticks * 100U) / period_ticks;
}

static void handle_ic_capture(
    PwmChannel *ch, uint32_t captured, uint32_t channel, GPIO_TypeDef *port, uint16_t pin)
{
    bool is_high = (HAL_GPIO_ReadPin(port, pin) == GPIO_PIN_SET);
    uint32_t delta;

    if (is_high) /* RISING EDGE — end of low phase */
    {
        if (ch->fall_captured)
        {
            delta = calculate_delta(
                captured, ch->fall_timestamp, _capture_tim->hal_handle.Instance->ARR);

            if (delta < MIN_LEVEL_TICKS || delta > MAX_LEVEL_TICKS)
            {
                reset_channel_state(ch, channel);
                return;
            }

            ch->low_level_ticks = delta;
            uint32_t raw_period = ch->pulse_ticks + ch->low_level_ticks;

            if (ch->period_ticks == 0)
            {
                ch->period_ticks = raw_period;
            }
            else
            {
                ch->period_ticks = (raw_period + 3 * ch->period_ticks) / 4;
            }

            if (ch->period_ticks < MIN_PERIOD_TICKS || ch->period_ticks > MAX_PERIOD_TICKS)
            {
                reset_channel_state(ch, channel);
                return;
            }

            uint32_t st_diff = (ch->period_ticks > ch->previous_period_ticks)
                ? (ch->period_ticks - ch->previous_period_ticks)
                : (ch->previous_period_ticks - ch->period_ticks);

            if (st_diff <= STABILITY_THRESHOLD)
            {
                ch->period_stable_counter++;
            }
            else
            {
                ch->period_stable_counter = 0;
            }

            if (ch->period_stable_counter >= STABILITY_REQUIRED_COUNT
                && ch->pulse_stable_counter >= STABILITY_REQUIRED_COUNT)
            {
                ch->new_data_ready = true;
            }

            ch->previous_period_ticks = ch->period_ticks;
            ch->fall_captured = false;
        }

        ch->rise_timestamp = captured;
        ch->rise_captured = true;
        ch->last_capture_ms = HAL_GetTick();
    }
    else /* FALLING EDGE — end of high phase */
    {
        if (ch->rise_captured)
        {
            delta = calculate_delta(
                captured, ch->rise_timestamp, _capture_tim->hal_handle.Instance->ARR);

            if (delta < MIN_LEVEL_TICKS || delta > MAX_LEVEL_TICKS)
            {
                reset_channel_state(ch, channel);
                return;
            }

            if (ch->pulse_ticks == 0)
            {
                ch->pulse_ticks = delta;
            }
            else
            {
                ch->pulse_ticks = (delta + 3 * ch->pulse_ticks) / 4;
            }

            uint32_t p_diff = (ch->pulse_ticks > ch->previous_pulse_ticks)
                ? (ch->pulse_ticks - ch->previous_pulse_ticks)
                : (ch->previous_pulse_ticks - ch->pulse_ticks);

            if (p_diff <= STABILITY_THRESHOLD)
            {
                ch->pulse_stable_counter++;
            }
            else
            {
                ch->pulse_stable_counter = 0;
            }
            ch->previous_pulse_ticks = ch->pulse_ticks;
            ch->fall_captured = true;
        }

        ch->fall_timestamp = captured;
        ch->last_capture_ms = HAL_GetTick();
    }
}

/* ── HAL callback (override) ───────────────────────────────────────────────── */

void HAL_TIM_IC_CaptureCallback(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == TIM2)
    {
        uint32_t captured_val;

        if (htim->Channel == HAL_TIM_ACTIVE_CHANNEL_3)
        {
            captured_val = HAL_TIM_ReadCapturedValue(htim, TIM_CHANNEL_3);
            handle_ic_capture(&pwmChannelA, captured_val, TIM_CHANNEL_3, GPIOB, GPIO_PIN_10);
        }
        else if (htim->Channel == HAL_TIM_ACTIVE_CHANNEL_4)
        {
            captured_val = HAL_TIM_ReadCapturedValue(htim, TIM_CHANNEL_4);
            handle_ic_capture(&pwmChannelB, captured_val, TIM_CHANNEL_4, GPIOB, GPIO_PIN_11);
        }
    }
}

/* ── Public task / getters ─────────────────────────────────────────────────── */

void pwm_repeater_task(void)
{
    process_channel_update(&pwmChannelA, &pwmOutputA, GPIOB, GPIO_PIN_10);
    process_channel_update(&pwmChannelB, &pwmOutputB, GPIOB, GPIO_PIN_11);
}

uint32_t get_ticks(void)
{
    return pwmChannelA.period_ticks;
}

uint32_t pwm_get_frequency_a(void)
{
    return calculate_frequency(pwmChannelA.period_ticks);
}

uint32_t pwm_get_duty_a(void)
{
    return calculate_duty_pct(pwmChannelA.period_ticks, pwmChannelA.pulse_ticks);
}

uint32_t pwm_get_frequency_b(void)
{
    return calculate_frequency(pwmChannelB.period_ticks);
}

uint32_t pwm_get_duty_b(void)
{
    return calculate_duty_pct(pwmChannelB.period_ticks, pwmChannelB.pulse_ticks);
}
