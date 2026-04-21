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

#define OUTPUT_FREQ_HZ 160U
#define OUTPUT_TIMER_HZ 1000000U
#define OUTPUT_PERIOD_TICKS (OUTPUT_TIMER_HZ / OUTPUT_FREQ_HZ) /* 6250 */
#define OUTPUT_ARR (OUTPUT_PERIOD_TICKS - 1U) /* 6249 */

/* ── Module-private timer handles (set by pwm_repeater_init) ───────────────── */
static Tim *_capture_tim = NULL;
static Tim *_out_a_tim = NULL;
static Tim *_out_b_tim = NULL;

/* ── Global instances ──────────────────────────────────────────────────────── */
PwmChannel pwmChannelA = {.rise_captured = false};
PwmChannel pwmChannelB = {.rise_captured = false};
PwmOutput pwmOutputA = {.tim = NULL, .channel = TIM_CHANNEL_1, .throttle_val = 50};
PwmOutput pwmOutputB = {.tim = NULL, .channel = TIM_CHANNEL_1, .throttle_val = 50};

/* ── Forward declarations ──────────────────────────────────────────────────── */
static void handle_ic_capture(
    PwmChannel *ch, uint32_t captured, uint32_t channel, GPIO_TypeDef *port, uint16_t pin);
static void reset_channel_state(PwmChannel *ch, uint32_t channel);
static uint32_t calculate_output_pulse(PwmOutput *out);
static void init_channel_struct(PwmChannel *ch);
static void apply_output_to_hardware(PwmOutput *out, uint32_t active_pulse);
static void process_channel_update(PwmChannel *ch, PwmOutput *out);
static uint32_t calculate_frequency(uint32_t period_ticks);
static uint32_t calculate_duty_pct(uint32_t period_ticks, uint32_t pulse_ticks);

/* ── Public API ────────────────────────────────────────────────────────────── */

void pwm_set_throttle_a(uint32_t limit_pct)
{
    pwmOutputA.throttle_val = (limit_pct > 100) ? 100 : limit_pct;
}

void pwm_set_throttle_b(uint32_t limit_pct)
{
    pwmOutputB.throttle_val = (limit_pct > 100) ? 100 : limit_pct;
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
    /* INIT ONLY — safe here because ISRs are enabled below (HAL_TIM_IC_Start_IT).
     * Forces ARPE preload into active ARR immediately; without this the old CubeMX
     * ARR stays active until the first natural UEV, risking a bad first capture. */
    _capture_tim->hal_handle.Instance->EGR = TIM_EGR_UG;

    HAL_TIM_PWM_Start(&_out_a_tim->hal_handle, TIM_CHANNEL_1);
    HAL_TIM_PWM_Start(&_out_b_tim->hal_handle, TIM_CHANNEL_1);
    HAL_TIM_IC_Start_IT(&_capture_tim->hal_handle, TIM_CHANNEL_3);
    HAL_TIM_IC_Start_IT(&_capture_tim->hal_handle, TIM_CHANNEL_4);

    pwm_set_throttle_a(50);
    pwm_set_throttle_b(50);
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
    uint32_t limit_ticks = (uint32_t)(((uint64_t)out->period_ticks * out->throttle_val) / 100U);
    return (out->pulse_ticks < limit_ticks) ? out->pulse_ticks : limit_ticks;
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
    if (out->period_ticks == 0)
    {
        return;
    }

    uint32_t active_ccr =
        (uint32_t)(((uint64_t)active_pulse * OUTPUT_PERIOD_TICKS) / out->period_ticks);

    if (active_ccr > OUTPUT_PERIOD_TICKS)
    {
        active_ccr = OUTPUT_PERIOD_TICKS;
    }

    /* BJT on output inverts: PA0 HIGH → LCD_PWM LOW.
     * To achieve desired duty cycle, we invert CCR so that
     * TIM output HIGH time = desired LCD_PWM LOW time. */
    uint32_t target_ccr = OUTPUT_PERIOD_TICKS - active_ccr;

    out->tim->hal_handle.Instance->ARR = OUTPUT_ARR;
    out->tim->hal_handle.Instance->CCR1 = target_ccr;
}

static void process_channel_update(PwmChannel *ch, PwmOutput *out)
{
    uint32_t now = HAL_GetTick();
    const uint32_t TIMEOUT_MS = 50;

    if (ch->new_data_ready)
    {
        out->period_ticks = ch->period_ticks;
        out->pulse_ticks = ch->low_level_ticks; /* DIM_PWM HIGH time (BJT-corrected) */

        uint32_t active_pulse = calculate_output_pulse(out);
        apply_output_to_hardware(out, active_pulse);

        ch->new_data_ready = false;
    }

    if ((now - ch->last_capture_ms) > TIMEOUT_MS)
    {
        /* No valid PWM edges for 50 ms — shut off output */
        out->tim->hal_handle.Instance->ARR = OUTPUT_ARR;
        out->tim->hal_handle.Instance->CCR1 = OUTPUT_ARR + 1U;
        init_channel_struct(ch);
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
    process_channel_update(&pwmChannelA, &pwmOutputA);
    process_channel_update(&pwmChannelB, &pwmOutputB);
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
    return calculate_duty_pct(pwmChannelA.period_ticks, pwmChannelA.low_level_ticks);
}

uint32_t pwm_get_frequency_b(void)
{
    return calculate_frequency(pwmChannelB.period_ticks);
}

uint32_t pwm_get_duty_b(void)
{
    return calculate_duty_pct(pwmChannelB.period_ticks, pwmChannelB.low_level_ticks);
}
