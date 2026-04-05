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

static void handle_ic_capture(PWM_Channel_t *ch, uint32_t captured, uint32_t channel, uint32_t ccr_offset);
static void reset_channel_state(PWM_Channel_t *ch, uint32_t channel);

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
    sConfigIC.ICPolarity = TIM_ICPOLARITY_RISING;
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
    pwmChA.rise_captured = false;
    pwmChA.fall_captured = false;
    pwmChA.period_ticks = 0;
    pwmChA.pulse_ticks = 0;
    pwmChA.low_level_ticks = 0;
    pwmChA.last_capture_ms = HAL_GetTick();
    pwmChA.new_data_ready = false;
    pwmChA.period_stable_counter = 0;
    pwmChA.previous_period_ticks = 0;
    pwmChA.pulse_stable_counter = 0;
    pwmChA.previous_pulse_ticks = 0;

    pwmChB.rise_captured = false;
    pwmChB.fall_captured = false;
    pwmChB.period_ticks = 0;
    pwmChB.pulse_ticks = 0;
    pwmChB.low_level_ticks = 0;
    pwmChB.last_capture_ms = HAL_GetTick();
    pwmChB.new_data_ready = false;
    pwmChB.period_stable_counter = 0;
    pwmChB.previous_period_ticks = 0;
    pwmChB.pulse_stable_counter = 0;
    pwmChB.previous_pulse_ticks = 0;

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
    __HAL_TIM_SET_CAPTUREPOLARITY(&htim2, channel, TIM_ICPOLARITY_RISING);
}

/**
 * @brief Shared logic for handling Input Capture events.
 */
static void handle_ic_capture(PWM_Channel_t *ch, uint32_t captured, uint32_t channel, uint32_t ccr_offset)
{
    /* Read polarity from CCER register to decide if this is Rising or Falling */
    uint32_t poll_bit = (channel == TIM_CHANNEL_3) ? TIM_CCER_CC3P : TIM_CCER_CC4P;
    bool is_falling = (htim2.Instance->CCER & poll_bit) ? true : false;
    uint32_t delta;

    if (!is_falling) /* RISING EDGE captured (End of Low phase) */
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

        /* Switch to FALLING edge */
        __HAL_TIM_SET_CAPTUREPOLARITY(&htim2, channel, TIM_ICPOLARITY_FALLING);
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

        /* Switch back to RISING edge */
        __HAL_TIM_SET_CAPTUREPOLARITY(&htim2, channel, TIM_ICPOLARITY_RISING);
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
            handle_ic_capture(&pwmChA, captured_val, TIM_CHANNEL_3, 0x00);
        }
        else if (htim->Channel == HAL_TIM_ACTIVE_CHANNEL_4)
        {
            captured_val = HAL_TIM_ReadCapturedValue(htim, TIM_CHANNEL_4);
            handle_ic_capture(&pwmChB, captured_val, TIM_CHANNEL_4, 0x04);
        }
    }
}

/**
 * @brief Watchdog to zero outputs if signal is lost.
 */
void pwm_repeater_task(void)
{
    uint32_t now = HAL_GetTick();
    const uint32_t timeout_ms = 50;

    /* Update Channel A output */
    if (pwmChA.new_data_ready)
    {
        pwmOutA.period_ticks = pwmChA.period_ticks;
        pwmOutA.pulse_ticks = pwmChA.pulse_ticks;
        
        /* Apply throttling based on mode */
        uint32_t active_pulse;
        if (pwmOutA.throttle_mode == ThrottleMode_Scale)
        {
            active_pulse = (pwmOutA.pulse_ticks * pwmOutA.throttle_val) / 100U;
        }
        else /* ThrottleMode_Fixed */
        {
            active_pulse = (pwmOutA.period_ticks * pwmOutA.throttle_val) / 100U;
        }

        /* Constraint: Resulting DC must not exceed measured input DC */
        if (active_pulse > pwmOutA.pulse_ticks)
        {
            active_pulse = pwmOutA.pulse_ticks;
        }

        /* Safety capping (hard limit) */
        uint32_t cap_limit = (pwmOutA.period_ticks * pwmOutA.cap_factor_pct) / 100U;
        if (active_pulse > cap_limit)
        {
            active_pulse = cap_limit;
        }

        /* Apply to hardware (with division by 48 for 1MHz output timebase) */
        uint32_t output_period = (pwmOutA.period_ticks / 48U);
        uint32_t target_arr = (output_period > 0xFFFFU) ? 0xFFFFU : (output_period - 1);
        uint32_t target_ccr = (active_pulse / 48U);
        
        pwmOutA.htim->Instance->ARR = target_arr;
        pwmOutA.htim->Instance->CCR1 = target_ccr;
        
        pwmChA.new_data_ready = false;
    }

    /* Update Channel B output */
    if (pwmChB.new_data_ready)
    {
        pwmOutB.period_ticks = pwmChB.period_ticks;
        pwmOutB.pulse_ticks = pwmChB.pulse_ticks;

        /* Apply throttling based on mode */
        uint32_t active_pulse;
        if (pwmOutB.throttle_mode == ThrottleMode_Scale)
        {
            active_pulse = (pwmOutB.pulse_ticks * pwmOutB.throttle_val) / 100U;
        }
        else /* ThrottleMode_Fixed */
        {
            active_pulse = (pwmOutB.period_ticks * pwmOutB.throttle_val) / 100U;
        }

        /* Constraint: Resulting DC must not exceed measured input DC */
        if (active_pulse > pwmOutB.pulse_ticks)
        {
            active_pulse = pwmOutB.pulse_ticks;
        }

        /* Safety capping (hard limit) */
        uint32_t cap_limit = (pwmOutB.period_ticks * pwmOutB.cap_factor_pct) / 100U;
        if (active_pulse > cap_limit)
        {
            active_pulse = cap_limit;
        }

        /* Apply to hardware (with division by 48 for 1MHz output timebase) */
        uint32_t output_period = (pwmOutB.period_ticks / 48U);
        uint32_t target_arr = (output_period > 0xFFFFU) ? 0xFFFFU : (output_period - 1);
        uint32_t target_ccr = (active_pulse / 48U);

        pwmOutB.htim->Instance->ARR = target_arr;
        pwmOutB.htim->Instance->CCR1 = target_ccr;
        
        pwmChB.new_data_ready = false;
    }

    /* Watchdog logic */
    if ((now - pwmChA.last_capture_ms) > timeout_ms)
    {
        pwmOutA.htim->Instance->CCR1 = 0;
        pwmChA.rise_captured = false;
        pwmChA.fall_captured = false;
        pwmChA.new_data_ready = false;
        pwmChA.period_ticks = 0;
        pwmChA.pulse_ticks = 0;
        pwmChA.low_level_ticks = 0;
    }

    if ((now - pwmChB.last_capture_ms) > timeout_ms)
    {
        pwmOutB.htim->Instance->CCR1 = 0;
        pwmChB.rise_captured = false;
        pwmChB.fall_captured = false;
        pwmChB.new_data_ready = false;
        pwmChB.period_ticks = 0;
        pwmChB.pulse_ticks = 0;
        pwmChB.low_level_ticks = 0;
    }
}


uint32_t get_ticks(void)
{
    return pwmChA.period_ticks;
}

uint32_t pwm_get_frequency_a(void)
{
    if (pwmChA.period_ticks == 0) return 0;
    return 48000000U / pwmChA.period_ticks;
}

uint32_t pwm_get_duty_a(void)
{
    if (pwmChA.period_ticks == 0) return 0;
    return (pwmChA.pulse_ticks * 100U) / pwmChA.period_ticks;
}

uint32_t pwm_get_frequency_b(void)
{
    if (pwmChB.period_ticks == 0) return 0;
    return 48000000U / pwmChB.period_ticks;
}

uint32_t pwm_get_duty_b(void)
{
    if (pwmChB.period_ticks == 0) return 0;
    return (pwmChB.pulse_ticks * 100U) / pwmChB.period_ticks;
}
