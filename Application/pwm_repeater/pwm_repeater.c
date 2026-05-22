/**
 * @file pwm_repeater.c
 * @brief TIM2-based PWM Repeater with duty cycle capping.
 */

#include "pwm_repeater.h"
#include "critical.h"
#include "gpio.h"
#include "sys_time.h"
#include "stm32c0xx_hal.h"
#include <stdint.h>

/* ── Tuning constants ──────────────────────────────────────────────────────── */
#define TIMEOUT_TICKS 48000000U /* 1 second at 48 MHz */
#define MAX_DUTY_PCT 100U
#define IC_FILTER_VAL 0x04U
#define MIN_LEVEL_TICKS 10U
#define MAX_LEVEL_TICKS 1200000U

/* Accepted input-frequency window. Edges outside this range are rejected. */
#define CAPTURE_TIMER_HZ 1000000U
#define MIN_INPUT_FREQ_HZ 50U
#define MAX_INPUT_FREQ_HZ 300U
#define MIN_PERIOD_TICKS (CAPTURE_TIMER_HZ / MAX_INPUT_FREQ_HZ) /* 4000  ticks → 250 Hz */
#define MAX_PERIOD_TICKS (CAPTURE_TIMER_HZ / MIN_INPUT_FREQ_HZ) /* 10000 ticks → 100 Hz */

#define ARR_UPDATE_THRESHOLD 100U
#define STABILITY_THRESHOLD 150U /* max tick deviation between consecutive measurements; resets counter if exceeded */
#define STABILITY_REQUIRED_COUNT 3U

#define OUTPUT_FREQ_HZ 160U
#define OUTPUT_TIMER_HZ 1000000U
#define OUTPUT_PERIOD_TICKS (OUTPUT_TIMER_HZ / OUTPUT_FREQ_HZ) /* 6250 */
#define OUTPUT_ARR (OUTPUT_PERIOD_TICKS - 1U) /* 6249 */

/* ── Module-private timer handles (set by pwm_repeater_init) ───────────────── */
static Tim *_capture_tim = NULL;
static Tim *_out_a_tim = NULL;
static Tim *_out_b_tim = NULL;

/* ── Input pin handles (PB10 = ch A, PB11 = ch B) ─────────────────────────── */
static Gpio _ic_pin_a;
static Gpio _ic_pin_b;

/* ── Global instances ──────────────────────────────────────────────────────── */
PwmChannel pwmChannelA = {.rise_captured = false};
PwmChannel pwmChannelB = {.rise_captured = false};
PwmOutput pwmOutputA = {.tim = NULL, .channel = TIM_CHANNEL_1, .throttle_val = 100, .period_valid = false};
PwmOutput pwmOutputB = {.tim = NULL, .channel = TIM_CHANNEL_1, .throttle_val = 100, .period_valid = false};

/* ── Forward declarations ──────────────────────────────────────────────────── */
static void handle_ic_capture(
    PwmChannel *ch, uint32_t captured, uint32_t channel, const Gpio *gpio);
static void reset_channel_state(PwmChannel *ch, uint32_t channel);
static uint32_t calculate_output_pulse(PwmOutput *out);
static void init_channel_struct(PwmChannel *ch);
static void apply_output_to_hardware(PwmOutput *out, uint32_t active_pulse);
static void apply_throttled_output(PwmOutput *out);
static void process_channel_update(PwmChannel *ch, PwmOutput *out, const Gpio *gpio);
static uint32_t calculate_frequency(uint32_t period_ticks);
static uint32_t calculate_duty_pct(uint32_t period_ticks, uint32_t pulse_ticks);

/* ── Public API ────────────────────────────────────────────────────────────── */

void pwm_set_throttle_a(uint8_t limit_pct)
{
    pwmOutputA.throttle_val = (limit_pct > 100U) ? 100U : limit_pct;
}

void pwm_set_throttle_b(uint8_t limit_pct)
{
    pwmOutputB.throttle_val = (limit_pct > 100U) ? 100U : limit_pct;
}

void pwm_repeater_init(Tim *capture_tim, Tim *out_a_tim, Tim *out_b_tim)
{
    _capture_tim = capture_tim;
    _out_a_tim = out_a_tim;
    _out_b_tim = out_b_tim;

    pwmOutputA.tim = out_a_tim;
    pwmOutputB.tim = out_b_tim;

    /* 1. Bind port/pin for edge-polarity reads (pins stay in TIM2-AF mode set by CubeMX) */
    _ic_pin_a = (Gpio){.port = GPIOB, .pin = GPIO_PIN_10};
    _ic_pin_b = (Gpio){.port = GPIOB, .pin = GPIO_PIN_11};

    /* 2. Configure input capture channels (also starts capture + IRQ) */
    tim_ic_config_channel(_capture_tim, 3, TIM_ICPOLARITY_BOTHEDGE, IC_FILTER_VAL);
    tim_ic_config_channel(_capture_tim, 4, TIM_ICPOLARITY_BOTHEDGE, IC_FILTER_VAL);

    /* 3. Enable ARR preload on capture timer and both output timers */
    _capture_tim->hal_handle.Instance->CR1 |= TIM_CR1_ARPE;

    _out_a_tim->hal_handle.Instance->CR1 |= TIM_CR1_ARPE;
    _out_a_tim->hal_handle.Instance->PSC = 47; /* 48 MHz → 1 MHz (1 µs resolution) */
    __HAL_TIM_ENABLE_OCxPRELOAD(&_out_a_tim->hal_handle, TIM_CHANNEL_1);

    _out_b_tim->hal_handle.Instance->CR1 |= TIM_CR1_ARPE;
    _out_b_tim->hal_handle.Instance->PSC = 47;
    __HAL_TIM_ENABLE_OCxPRELOAD(&_out_b_tim->hal_handle, TIM_CHANNEL_1);

    /* 4. Reset channel state */
    init_channel_struct(&pwmChannelA);
    init_channel_struct(&pwmChannelB);

    /* 5. Force ARR update and start peripherals */
    _capture_tim->hal_handle.Instance->ARR = UINT32_MAX;
    /* INIT ONLY — safe here because CC interrupts are not yet enabled (tim_ic_enable_ch_irq
     * is called below, after this write). Forces ARPE preload into active ARR immediately;
     * without this the old CubeMX ARR stays active until the first natural UEV, risking a
     * bad first capture. */
    _capture_tim->hal_handle.Instance->EGR = TIM_EGR_UG;

    tim_pwm_start(_out_a_tim, 1);
    tim_pwm_start(_out_b_tim, 1);
    tim_ic_enable_ch_irq(_capture_tim, 3);
    tim_ic_enable_ch_irq(_capture_tim, 4);

    pwm_set_throttle_a(100);
    pwm_set_throttle_b(100);
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
    ch->last_capture_ms = millis();
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

static void apply_throttled_output(PwmOutput *out)
{
    uint32_t active_pulse = calculate_output_pulse(out);
    apply_output_to_hardware(out, active_pulse);
}

static void process_channel_update(PwmChannel *ch, PwmOutput *out, const Gpio *gpio)
{
    millis_t now = millis();
    const uint32_t TIMEOUT_MS = 50;

    critical_enter();
    bool ready      = ch->new_data_ready;
    uint32_t period = ch->period_ticks;
    uint32_t pulse  = ch->low_level_ticks;
    ch->new_data_ready = false;
    millis_t last_capture = ch->last_capture_ms;
    critical_exit();

   if (ready)
    {
        out->period_ticks = period;
        out->pulse_ticks  = pulse; /* DIM_PWM HIGH time (BJT-corrected) */
        out->period_valid = true;

        apply_throttled_output(out);
    }

    if ((now > last_capture + TIMEOUT_MS))
    {
        /* No edges for 50 ms: flat DC. Read pin to determine level.
         * Input BJT inverts: pin LOW = DIM_PWM 100%, pin HIGH = DIM_PWM 0%. */
        if (gpio_read(gpio) == GPIO_PIN_RESET)
        {
            if (out->period_valid)
            {
                /* Known period — apply throttled 100% using last seen frequency */
                out->pulse_ticks = out->period_ticks;
                apply_throttled_output(out);
            }
            else
            {
                /* Cold 100% DC — no frequency seen yet. Apply throttle directly against
                 * output period; no input period to scale against. */
                uint32_t cold_pulse = (OUTPUT_PERIOD_TICKS * out->throttle_val) / 100U;
                uint32_t target_ccr = OUTPUT_PERIOD_TICKS - cold_pulse;
                out->tim->hal_handle.Instance->ARR = OUTPUT_ARR;
                out->tim->hal_handle.Instance->CCR1 = target_ccr;
            }
        }
        else
        {
            /* 0% DC / signal lost */
            out->tim->hal_handle.Instance->ARR = OUTPUT_ARR;
            out->tim->hal_handle.Instance->CCR1 = OUTPUT_ARR + 1U;
            out->period_ticks = 0;
            out->pulse_ticks = 0;
            out->period_valid = false;
        }
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

static void handle_ic_capture(PwmChannel *ch, uint32_t captured, uint32_t channel, const Gpio *gpio)
{
    bool is_high = (gpio_read(gpio) == GPIO_PIN_SET);
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
        ch->last_capture_ms = millis();
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
        ch->last_capture_ms = millis();
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
            captured_val = tim_ic_get_channel(_capture_tim, 3);
            handle_ic_capture(&pwmChannelA, captured_val, TIM_CHANNEL_3, &_ic_pin_a);
        }
        else if (htim->Channel == HAL_TIM_ACTIVE_CHANNEL_4)
        {
            captured_val = tim_ic_get_channel(_capture_tim, 4);
            handle_ic_capture(&pwmChannelB, captured_val, TIM_CHANNEL_4, &_ic_pin_b);
        }
    }
}

/* ── Public task / getters ─────────────────────────────────────────────────── */

void pwm_repeater_task(void)
{
    process_channel_update(&pwmChannelA, &pwmOutputA, &_ic_pin_a);
    process_channel_update(&pwmChannelB, &pwmOutputB, &_ic_pin_b);
}

uint32_t pwm_get_frequency_a(void)
{
    return calculate_frequency(pwmChannelA.period_ticks);
}

uint32_t pwm_get_duty_a(void)
{
    if (pwmChannelA.period_ticks == 0)
    {
        /* Flat DC — no PWM signal detected. Read pin directly.
         * Input BJT inverts: pin LOW = 100% DC, pin HIGH = 0% DC. */
        return (gpio_read(&_ic_pin_a) == GPIO_PIN_RESET) ? 100 : 0;
    }
    return calculate_duty_pct(pwmChannelA.period_ticks, pwmChannelA.low_level_ticks);
}

uint32_t pwm_get_frequency_b(void)
{
    return calculate_frequency(pwmChannelB.period_ticks);
}

uint32_t pwm_get_duty_b(void)
{
    if (pwmChannelB.period_ticks == 0)
    {
        /* Flat DC — no PWM signal detected. Read pin directly.
         * Input BJT inverts: pin LOW = 100% DC, pin HIGH = 0% DC. */
        return (gpio_read(&_ic_pin_b) == GPIO_PIN_RESET) ? 100 : 0;
    }
    return calculate_duty_pct(pwmChannelB.period_ticks, pwmChannelB.low_level_ticks);
}

uint32_t pwm_get_output_duty_a(void)
{
    if (pwmOutputA.period_ticks == 0U)
    {
        /* Flat DC — no PWM signal detected. Read input pin directly.
         * Input BJT inverts: pin LOW = 100% output, pin HIGH = 0% output. */
        return (gpio_read(&_ic_pin_a) == GPIO_PIN_RESET) ? 100U : 0U;
    }
    uint32_t limit =
        (uint32_t)(((uint64_t)pwmOutputA.period_ticks * pwmOutputA.throttle_val) / 100U);
    uint32_t active = (pwmOutputA.pulse_ticks < limit) ? pwmOutputA.pulse_ticks : limit;
    return (uint32_t)((uint64_t)active * 100ULL / pwmOutputA.period_ticks);
}

uint32_t pwm_get_output_duty_b(void)
{
    if (pwmOutputB.period_ticks == 0U)
    {
        /* Flat DC — no PWM signal detected. Read input pin directly.
         * Input BJT inverts: pin LOW = 100% output, pin HIGH = 0% output. */
        return (gpio_read(&_ic_pin_b) == GPIO_PIN_RESET) ? 100U : 0U;
    }
    uint32_t limit =
        (uint32_t)(((uint64_t)pwmOutputB.period_ticks * pwmOutputB.throttle_val) / 100U);
    uint32_t active = (pwmOutputB.pulse_ticks < limit) ? pwmOutputB.pulse_ticks : limit;
    return (uint32_t)((uint64_t)active * 100ULL / pwmOutputB.period_ticks);
}
