/**
 * @file pwm_repeater.c
 * @brief Implementation of the TIM2-based PWM Repeater with duty cycle capping.
 */

#include "pwm_repeater.h"
#include <stdint.h>

/* Tuning Constants */
#define TIMEOUT_TICKS           48000000U /* 1 second at 48MHz */
#define MAX_DUTY_PCT            100U
#define IC_FILTER_VAL           0x04U
#define MIN_LEVEL_TICKS         100U
#define MAX_LEVEL_TICKS         1200000U
#define MIN_PERIOD_TICKS        1000U
#define MAX_PERIOD_TICKS        1500000U
#define ARR_UPDATE_THRESHOLD    100U
#define STABILITY_THRESHOLD     100U
#define STABILITY_REQUIRED_COUNT 3U

/* Static Timer Handles */
extern TIM_HandleTypeDef htim2;
extern TIM_HandleTypeDef htim16;
extern TIM_HandleTypeDef htim17;

/* Global instances */
PWM_Channel_t pwmChA = { .rise_captured = false };
PWM_Channel_t pwmChB = { .rise_captured = false };
PWM_Output_t  pwmOutA = { .htim = &htim16, .channel = TIM_CHANNEL_1, .cap_factor_pct = 100, .throttle_val = 100, .throttle_mode = ThrottleMode_Scale };
PWM_Output_t  pwmOutB = { .htim = &htim17, .channel = TIM_CHANNEL_1, .cap_factor_pct = 100, .throttle_val = 100, .throttle_mode = ThrottleMode_Scale };

static void handle_ic_capture(PWM_Channel_t *ch, uint32_t captured, uint32_t channel, GPIO_TypeDef *port, uint16_t pin);
static void reset_channel_state(PWM_Channel_t *ch, uint32_t channel);
static uint32_t calculate_output_pulse(PWM_Output_t *out);
static void init_channel_struct(PWM_Channel_t *ch);
static void apply_output_to_hardware(PWM_Output_t *out, uint32_t active_pulse);
static void process_channel_update(PWM_Channel_t *ch, PWM_Output_t *out, GPIO_TypeDef *port, uint16_t pin);
static uint32_t calculate_frequency(uint32_t period_ticks);
static uint32_t calculate_duty_pct(uint32_t period_ticks, uint32_t pulse_ticks);

void pwm_set_throttle_a(uint32_t val, ThrottleMode mode)
{
    if (val > 100)
    {
        val = 100;
    }
    pwmOutA.throttle_val = val;
    pwmOutA.throttle_mode = mode;
}

void pwm_set_throttle_b(uint32_t val, ThrottleMode mode)
{
    if (val > 100)
    {
        val = 100;
    }
    pwmOutB.throttle_val = val;
    pwmOutB.throttle_mode = mode;
}

/**
 * @brief Configures TIM2 for PWM generation and Input Capture.
 * Ensures CCR preload is enabled and starts interrupts.
 */
void pwm_repeater_init(void)
{
    TIM_IC_InitTypeDef sConfigIC = {0};

    /* 1. Configure Input Capture filters using HAL (safer than register writes) */
    sConfigIC.ICPolarity = TIM_ICPOLARITY_BOTHEDGE;
    sConfigIC.ICSelection = TIM_ICSELECTION_DIRECTTI;
    sConfigIC.ICPrescaler = TIM_ICPSC_DIV1;
    sConfigIC.ICFilter = IC_FILTER_VAL;

    HAL_TIM_IC_ConfigChannel(&htim2, &sConfigIC, TIM_CHANNEL_3);
    HAL_TIM_IC_ConfigChannel(&htim2, &sConfigIC, TIM_CHANNEL_4);

    /* 2. Enable Preload for both ARR and CCR. */
    htim2.Instance->CR1 |= TIM_CR1_ARPE;
    
    htim16.Instance->CR1 |= TIM_CR1_ARPE;
    htim16.Instance->PSC = 47; /* 48MHz -> 1MHz (1us resolution) */
    __HAL_TIM_ENABLE_OCxPRELOAD(&htim16, TIM_CHANNEL_1);
    
    htim17.Instance->CR1 |= TIM_CR1_ARPE;
    htim17.Instance->PSC = 47; /* 48MHz -> 1MHz (1us resolution) */
    __HAL_TIM_ENABLE_OCxPRELOAD(&htim17, TIM_CHANNEL_1);

    /* 3. Reset state */
    init_channel_struct(&pwmChA);
    init_channel_struct(&pwmChB);

    /* 4. Start Peripheral CC Interrupts and Outputs */
    htim2.Instance->ARR = UINT32_MAX;
    htim2.Instance->EGR = TIM_EGR_UG;
    
    HAL_TIM_PWM_Start(&htim16, TIM_CHANNEL_1);
    HAL_TIM_PWM_Start(&htim17, TIM_CHANNEL_1);
    HAL_TIM_IC_Start_IT(&htim2, TIM_CHANNEL_3);
    HAL_TIM_IC_Start_IT(&htim2, TIM_CHANNEL_4);

    pwm_set_throttle_a(50, ThrottleMode_Fixed); /* Default to 50% throttle limit */
    pwm_set_throttle_b(100, ThrottleMode_Scale); /* Default to 100% (pass-through) */
}




/**
 * @brief Calculates the time difference between two captures, accounting for timer rollover at ARR.
 * @param current Current capture value.
 * @param previous Previous capture value.
 * @param arr Timer Auto-Reload Register value.
 * @return Delta ticks.
 */
static uint32_t calculate_delta(uint32_t current, uint32_t previous, uint32_t arr)
{
    uint32_t delta;

    if (current >= previous)
    {
        delta = current - previous;
    }
    else
    {
        delta = (arr - previous) + current + 1;
    }

    return delta;
}

/**
 * @brief Calculates the pulse duration after applying throttle and capping.
 * @param out Pointer to output configuration.
 * @return Active pulse duration in ticks.
 */
static uint32_t calculate_output_pulse(PWM_Output_t *out)
{
    uint32_t active_pulse;

    if (out->throttle_mode == ThrottleMode_Scale)
    {
        active_pulse = (out->pulse_ticks * out->throttle_val) / 100U;
    }
    else /* ThrottleMode_Fixed */
    {
        active_pulse = (out->period_ticks * out->throttle_val) / 100U;
    }

    /* Constraint: Resulting DC must not exceed measured input DC */
    if (active_pulse > out->pulse_ticks)
    {
        active_pulse = out->pulse_ticks;
    }

    /* Safety capping (hard limit) */
    uint32_t cap_limit = (out->period_ticks * out->cap_factor_pct) / 100U;
    if (active_pulse > cap_limit)
    {
        active_pulse = cap_limit;
    }
    
    return active_pulse;
}

/**
 * @brief Zeroes out a channel info structure.
 */
static void init_channel_struct(PWM_Channel_t *ch)
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

/**
 * @brief Converts tick durations to hardware register values and applies them.
 */
static void apply_output_to_hardware(PWM_Output_t *out, uint32_t active_pulse)
{
    /* Apply to hardware (with division by 48 for 1MHz output timebase) */
    uint32_t output_period = (out->period_ticks / 48U);
    uint32_t target_arr = (output_period > 0xFFFFU) ? 0xFFFFU : (output_period > 0 ? output_period - 1 : 0);
    uint32_t target_ccr = (active_pulse / 48U);

    /* Cap target_ccr to 100% duty cycle, ensuring CCR > ARR for glitch-free 100% DC. */
    if (target_ccr > target_arr)
    {
        if (target_arr == 0xFFFFU)
        {
            target_arr = 0xFFFEU;
        }
        target_ccr = target_arr + 1;
    }

    out->htim->Instance->ARR = target_arr;
    out->htim->Instance->CCR1 = target_ccr;
}

/**
 * @brief Shared logic for updating outputs and running the watchdog.
 */
static void process_channel_update(PWM_Channel_t *ch, PWM_Output_t *out, GPIO_TypeDef *port, uint16_t pin)
{
    uint32_t now = HAL_GetTick();
    const uint32_t timeout_ms = 50;

    if (ch->new_data_ready)
    {
        out->period_ticks = ch->period_ticks;
        out->pulse_ticks = ch->pulse_ticks;

        uint32_t active_pulse = calculate_output_pulse(out);
        apply_output_to_hardware(out, active_pulse);

        ch->new_data_ready = false;
    }

    /* Watchdog logic */
    if ((now - ch->last_capture_ms) > timeout_ms)
    {
        if (HAL_GPIO_ReadPin(port, pin) == GPIO_PIN_SET)
        {
            if (ch->period_ticks == 0)
            {
                /* Pure 100% DC - frequency unknown, set to lowest freq with no reload glitches */
                out->htim->Instance->ARR = 0xFFFE;
                out->htim->Instance->CCR1 = 0xFFFF;
            }
            else
            {
                /* 100% DC - use past known period and apply throttle */
                ch->pulse_ticks = ch->period_ticks;
                out->period_ticks = ch->period_ticks;
                out->pulse_ticks = ch->pulse_ticks;

                uint32_t active_pulse = calculate_output_pulse(out);
                apply_output_to_hardware(out, active_pulse);
            }
        }
        else
        {
            /* 0% DC / Signal Lost */
            out->htim->Instance->CCR1 = 0;
            init_channel_struct(ch);
        }
    }
}

/**
 * @brief Resets the capture state for a channel and sets polarity to rising.
 */
static void reset_channel_state(PWM_Channel_t *ch, uint32_t channel)
{
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

/**
 * @brief Computes frequency from period ticks.
 */
static uint32_t calculate_frequency(uint32_t period_ticks)
{
    if (period_ticks == 0)
    {
        return 0;
    }
    return 48000000U / period_ticks;
}

/**
 * @brief Computes duty cycle percentage.
 */
static uint32_t calculate_duty_pct(uint32_t period_ticks, uint32_t pulse_ticks)
{
    if (period_ticks == 0)
    {
        return 0;
    }
    return (pulse_ticks * 100U) / period_ticks;
}

/**
 * @brief Shared logic for handling Input Capture events.
 */
static void handle_ic_capture(PWM_Channel_t *ch, uint32_t captured, uint32_t channel, GPIO_TypeDef *port, uint16_t pin)
{
    /* With BOTHEDGE, we detect Rising vs Falling by checking the current pin level.
       At 150Hz-1kHz, the CPU entry time is fast enough that the level is stable. */
    bool is_high = (HAL_GPIO_ReadPin(port, pin) == GPIO_PIN_SET);
    uint32_t delta;

    if (is_high) /* RISING EDGE captured (End of Low phase) */
    {
        if (ch->fall_captured)
        {
            /* Low phase = current_rise - previous_fall */
            delta = calculate_delta(captured, ch->fall_timestamp, htim2.Instance->ARR);

            if (delta < MIN_LEVEL_TICKS || delta > MAX_LEVEL_TICKS)
            {
                reset_channel_state(ch, channel);
                return;
            }

            ch->low_level_ticks = delta;
            
            /* Period = High phase + Low phase. High phase (pulse_ticks) was already validated. */
            uint32_t raw_period = ch->pulse_ticks + ch->low_level_ticks;

            if (ch->period_ticks == 0)
            {
                ch->period_ticks = raw_period;
            }
            else
            {
                /* EMA Filter: alpha = 0.25 */
                ch->period_ticks = (raw_period + 3 * ch->period_ticks) / 4;
            }

            if (ch->period_ticks < MIN_PERIOD_TICKS || ch->period_ticks > MAX_PERIOD_TICKS)
            {
                reset_channel_state(ch, channel);
                return;
            }
            
            uint32_t st_diff = (ch->period_ticks > ch->previous_period_ticks) ? 
                               (ch->period_ticks - ch->previous_period_ticks) : 
                               (ch->previous_period_ticks - ch->period_ticks);

            if (st_diff <= STABILITY_THRESHOLD)
            {
                ch->period_stable_counter++;
            }
            else
            {
                ch->period_stable_counter = 0;
            }
            if (ch->period_stable_counter >= STABILITY_REQUIRED_COUNT && 
                ch->pulse_stable_counter >= STABILITY_REQUIRED_COUNT)
            {
                ch->new_data_ready = true;
            }

            ch->previous_period_ticks = ch->period_ticks;
            
            /* Reset sequence tracker - wait for next high phase */
            ch->fall_captured = false;
        }
        
        ch->rise_timestamp = captured;
        ch->rise_captured = true;
        ch->last_capture_ms = HAL_GetTick();
    }
    else /* FALLING EDGE captured (End of High phase) */
    {
        if (ch->rise_captured)
        {
            /* High phase = current_fall - previous_rise */
            delta = calculate_delta(captured, ch->rise_timestamp, htim2.Instance->ARR);

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
                /* EMA Filter: alpha = 0.25 */
                ch->pulse_ticks = (delta + 3 * ch->pulse_ticks) / 4;
            }

            /* Pulse stability debounce */
            uint32_t p_diff = (ch->pulse_ticks > ch->previous_pulse_ticks) ? 
                              (ch->pulse_ticks - ch->previous_pulse_ticks) : 
                              (ch->previous_pulse_ticks - ch->pulse_ticks);

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

/**
 * @brief HAL Override for Input Capture callback.
 */
void HAL_TIM_IC_CaptureCallback(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == TIM2)
    {
        uint32_t captured_val;
        
        if (htim->Channel == HAL_TIM_ACTIVE_CHANNEL_3)
        {
            captured_val = HAL_TIM_ReadCapturedValue(htim, TIM_CHANNEL_3);
            handle_ic_capture(&pwmChA, captured_val, TIM_CHANNEL_3, GPIOB, GPIO_PIN_10);
        }
        else if (htim->Channel == HAL_TIM_ACTIVE_CHANNEL_4)
        {
            captured_val = HAL_TIM_ReadCapturedValue(htim, TIM_CHANNEL_4);
            handle_ic_capture(&pwmChB, captured_val, TIM_CHANNEL_4, GPIOB, GPIO_PIN_11);
        }
    }
}

/**
 * @brief Watchdog to zero outputs if signal is lost.
 */
void pwm_repeater_task(void)
{
    process_channel_update(&pwmChA, &pwmOutA, GPIOB, GPIO_PIN_10);
    process_channel_update(&pwmChB, &pwmOutB, GPIOB, GPIO_PIN_11);
}


uint32_t get_ticks(void)
{
    return pwmChA.period_ticks;
}

uint32_t pwm_get_frequency_a(void)
{
    return calculate_frequency(pwmChA.period_ticks);
}

uint32_t pwm_get_duty_a(void)
{
    return calculate_duty_pct(pwmChA.period_ticks, pwmChA.pulse_ticks);
}

uint32_t pwm_get_frequency_b(void)
{
    return calculate_frequency(pwmChB.period_ticks);
}

uint32_t pwm_get_duty_b(void)
{
    return calculate_duty_pct(pwmChB.period_ticks, pwmChB.pulse_ticks);
}
