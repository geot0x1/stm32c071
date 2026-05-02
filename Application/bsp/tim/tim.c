#include "tim.h"
#include <string.h>

/**
 * @brief Get APBx clock frequency for a given timer instance
 *
 * Note: STM32C0 has only APB1. All timers run on PCLK1.
 */
static uint32_t tim_get_clock_freq(TIM_TypeDef *instance)
{
    (void)instance;
    return HAL_RCC_GetPCLK1Freq();
}

/**
 * @brief Enable peripheral clock for timer
 */
static void tim_enable_clock(TIM_TypeDef *instance)
{
    if (instance == TIM1)
    {
        __HAL_RCC_TIM1_CLK_ENABLE();
    }
    else if (instance == TIM2)
    {
        __HAL_RCC_TIM2_CLK_ENABLE();
    }
    else if (instance == TIM3)
    {
        __HAL_RCC_TIM3_CLK_ENABLE();
    }
    else if (instance == TIM14)
    {
        __HAL_RCC_TIM14_CLK_ENABLE();
    }
    else if (instance == TIM16)
    {
        __HAL_RCC_TIM16_CLK_ENABLE();
    }
    else if (instance == TIM17)
    {
        __HAL_RCC_TIM17_CLK_ENABLE();
    }
}

/**
 * @brief Enable NVIC interrupt for timer
 */
static void tim_enable_nvic(TIM_TypeDef *instance)
{
    if (instance == TIM1)
    {
        HAL_NVIC_EnableIRQ(TIM1_BRK_UP_TRG_COM_IRQn);
    }
    else if (instance == TIM2)
    {
        HAL_NVIC_EnableIRQ(TIM2_IRQn);
    }
    else if (instance == TIM3)
    {
        HAL_NVIC_EnableIRQ(TIM3_IRQn);
    }
    else if (instance == TIM14)
    {
        HAL_NVIC_EnableIRQ(TIM14_IRQn);
    }
    else if (instance == TIM16)
    {
        HAL_NVIC_EnableIRQ(TIM16_IRQn);
    }
    else if (instance == TIM17)
    {
        HAL_NVIC_EnableIRQ(TIM17_IRQn);
    }
}

/**
 * @brief Map 1-based channel index to HAL TIM_CHANNEL_x constant
 */
static uint32_t tim_channel(uint8_t channel)
{
    return (uint32_t)(channel - 1) << 2;
}

/* ── PWM output mode ──────────────────────────────────────────────────────── */

void tim_pwm_init(Tim *tim, TIM_TypeDef *instance, uint32_t freq_hz, uint8_t num_channels)
{
    memset(tim, 0, sizeof(Tim));

    tim->hal_handle.Instance = instance;
    tim->freq_hz = freq_hz;
    tim->num_channels = num_channels;

    uint32_t clk_freq = tim_get_clock_freq(instance);
    uint32_t arr = clk_freq / freq_hz;

    tim->hal_handle.Init.Prescaler = 0;
    tim->hal_handle.Init.CounterMode = TIM_COUNTERMODE_UP;
    tim->hal_handle.Init.Period = (arr > 0xFFFF) ? 0xFFFF : (arr - 1);
    tim->hal_handle.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    tim->hal_handle.Init.RepetitionCounter = 0;
    tim->hal_handle.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;

    HAL_TIM_PWM_Init(&tim->hal_handle);

    TIM_OC_InitTypeDef sConfigOC = {0};
    sConfigOC.OCMode = TIM_OCMODE_PWM1;
    sConfigOC.Pulse = 0;
    sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
    sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;

    for (uint8_t ch = 1; ch <= num_channels; ch++)
    {
        HAL_TIM_PWM_ConfigChannel(&tim->hal_handle, &sConfigOC, tim_channel(ch));
    }

    // Configure BreakDeadTime for advanced timers (TIM1, TIM16, TIM17)
    if (instance == TIM1 || instance == TIM16 || instance == TIM17)
    {
        TIM_BreakDeadTimeConfigTypeDef sBreakDeadTimeConfig = {0};
        sBreakDeadTimeConfig.OffStateRunMode = TIM_OSSR_DISABLE;
        sBreakDeadTimeConfig.OffStateIDLEMode = TIM_OSSI_DISABLE;
        sBreakDeadTimeConfig.LockLevel = TIM_LOCKLEVEL_OFF;
        sBreakDeadTimeConfig.DeadTime = 0;
        sBreakDeadTimeConfig.BreakState = TIM_BREAK_DISABLE;
        sBreakDeadTimeConfig.BreakPolarity = TIM_BREAKPOLARITY_HIGH;
        sBreakDeadTimeConfig.BreakFilter = 0;
        sBreakDeadTimeConfig.AutomaticOutput = TIM_AUTOMATICOUTPUT_DISABLE;
        if (instance == TIM1)
        {
            sBreakDeadTimeConfig.Break2State = TIM_BREAK2_DISABLE;
            sBreakDeadTimeConfig.Break2Polarity = TIM_BREAK2POLARITY_HIGH;
            sBreakDeadTimeConfig.Break2Filter = 0;
            sBreakDeadTimeConfig.BreakAFMode = TIM_BREAK_AFMODE_INPUT;
        }
        HAL_TIMEx_ConfigBreakDeadTime(&tim->hal_handle, &sBreakDeadTimeConfig);
    }
}

void tim_pwm_init_raw(
    Tim *tim, TIM_TypeDef *instance, uint32_t psc, uint32_t arr, uint8_t num_channels)
{
    memset(tim, 0, sizeof(Tim));

    tim->hal_handle.Instance = instance;
    tim->freq_hz = 0; // Not meaningful for raw mode
    tim->num_channels = num_channels;

    uint32_t clk_freq = tim_get_clock_freq(instance);
    (void)clk_freq; // Suppress unused warning — PSC is pre-computed by caller

    tim->hal_handle.Init.Prescaler = psc;
    tim->hal_handle.Init.CounterMode = TIM_COUNTERMODE_UP;
    tim->hal_handle.Init.Period = arr;
    tim->hal_handle.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    tim->hal_handle.Init.RepetitionCounter = 0;
    tim->hal_handle.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;

    HAL_TIM_PWM_Init(&tim->hal_handle);

    TIM_OC_InitTypeDef sConfigOC = {0};
    sConfigOC.OCMode = TIM_OCMODE_PWM1;
    sConfigOC.Pulse = 0;
    sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
    sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;

    for (uint8_t ch = 1; ch <= num_channels; ch++)
    {
        HAL_TIM_PWM_ConfigChannel(&tim->hal_handle, &sConfigOC, tim_channel(ch));
    }

    // Configure BreakDeadTime for advanced timers (TIM1, TIM16, TIM17)
    if (instance == TIM1 || instance == TIM16 || instance == TIM17)
    {
        TIM_BreakDeadTimeConfigTypeDef sBreakDeadTimeConfig = {0};
        sBreakDeadTimeConfig.OffStateRunMode = TIM_OSSR_DISABLE;
        sBreakDeadTimeConfig.OffStateIDLEMode = TIM_OSSI_DISABLE;
        sBreakDeadTimeConfig.LockLevel = TIM_LOCKLEVEL_OFF;
        sBreakDeadTimeConfig.DeadTime = 0;
        sBreakDeadTimeConfig.BreakState = TIM_BREAK_DISABLE;
        sBreakDeadTimeConfig.BreakPolarity = TIM_BREAKPOLARITY_HIGH;
        sBreakDeadTimeConfig.BreakFilter = 0;
        sBreakDeadTimeConfig.AutomaticOutput = TIM_AUTOMATICOUTPUT_DISABLE;
        if (instance == TIM1)
        {
            sBreakDeadTimeConfig.Break2State = TIM_BREAK2_DISABLE;
            sBreakDeadTimeConfig.Break2Polarity = TIM_BREAK2POLARITY_HIGH;
            sBreakDeadTimeConfig.Break2Filter = 0;
            sBreakDeadTimeConfig.BreakAFMode = TIM_BREAK_AFMODE_INPUT;
        }
        HAL_TIMEx_ConfigBreakDeadTime(&tim->hal_handle, &sBreakDeadTimeConfig);
    }
}

void tim_pwm_set_duty(Tim *tim, uint8_t channel, uint8_t duty_pct)
{
    if (channel < 1 || channel > tim->num_channels || duty_pct > 100)
    {
        return;
    }
    uint32_t pulse = (tim->hal_handle.Init.Period + 1) * duty_pct / 100;
    __HAL_TIM_SET_COMPARE(&tim->hal_handle, tim_channel(channel), pulse);
}

uint8_t tim_pwm_get_duty(Tim *tim, uint8_t channel)
{
    if (channel < 1 || channel > tim->num_channels)
    {
        return 0;
    }
    uint32_t ccr = __HAL_TIM_GET_COMPARE(&tim->hal_handle, tim_channel(channel));
    uint32_t period = tim->hal_handle.Instance->ARR + 1;
    return (uint8_t)((ccr * 100U) / period);
}

void tim_pwm_start(Tim *tim, uint8_t channel)
{
    if (channel < 1 || channel > tim->num_channels)
    {
        return;
    }
    HAL_TIM_PWM_Start(&tim->hal_handle, tim_channel(channel));
}

void tim_pwm_stop(Tim *tim, uint8_t channel)
{
    if (channel < 1 || channel > tim->num_channels)
    {
        return;
    }
    HAL_TIM_PWM_Stop(&tim->hal_handle, tim_channel(channel));
}

void tim_pwm_set_freq(Tim *tim, uint32_t freq_hz)
{
    tim->freq_hz = freq_hz;
    uint32_t clk_freq = tim_get_clock_freq(tim->hal_handle.Instance);
    uint32_t arr = (clk_freq / freq_hz) - 1;
    __HAL_TIM_SET_AUTORELOAD(&tim->hal_handle, arr);
    __HAL_TIM_CLEAR_FLAG(&tim->hal_handle, TIM_FLAG_UPDATE);
}

void tim_pwm_start_n(Tim *tim, uint8_t channel)
{
    HAL_TIMEx_PWMN_Start(&tim->hal_handle, tim_channel(channel));
}

void tim_pwm_stop_n(Tim *tim, uint8_t channel)
{
    HAL_TIMEx_PWMN_Stop(&tim->hal_handle, tim_channel(channel));
}

void tim_pwm_set_deadtime(Tim *tim, uint32_t deadtime_ns)
{
    uint32_t clk_freq = tim_get_clock_freq(tim->hal_handle.Instance);
    // t_DTS in nanoseconds (CKD=DIV1)
    uint32_t t_dts_ns = 1000000000UL / clk_freq;
    uint32_t dtg = deadtime_ns / t_dts_ns;
    if (dtg > 0x7F)
    {
        dtg = 0x7F;
    }

    TIM_BreakDeadTimeConfigTypeDef cfg = {0};
    cfg.OffStateRunMode = TIM_OSSR_DISABLE;
    cfg.OffStateIDLEMode = TIM_OSSI_DISABLE;
    cfg.LockLevel = TIM_LOCKLEVEL_OFF;
    cfg.DeadTime = (uint8_t)dtg;
    cfg.BreakState = TIM_BREAK_DISABLE;
    cfg.BreakPolarity = TIM_BREAKPOLARITY_HIGH;
    cfg.BreakFilter = 0;
    cfg.AutomaticOutput = TIM_AUTOMATICOUTPUT_DISABLE;
    if (tim->hal_handle.Instance == TIM1)
    {
        cfg.Break2State = TIM_BREAK2_DISABLE;
        cfg.Break2Polarity = TIM_BREAK2POLARITY_HIGH;
        cfg.Break2Filter = 0;
        cfg.BreakAFMode = TIM_BREAK_AFMODE_INPUT;
    }
    HAL_TIMEx_ConfigBreakDeadTime(&tim->hal_handle, &cfg);
}

/* ── Input capture mode ───────────────────────────────────────────────────── */

void tim_ic_init(Tim *tim, TIM_TypeDef *instance, uint32_t resolution_hz)
{
    memset(tim, 0, sizeof(Tim));

    tim->hal_handle.Instance = instance;
    tim->freq_hz = resolution_hz;

    tim_enable_clock(instance);

    uint32_t clk_freq = tim_get_clock_freq(instance);
    uint32_t psc = (clk_freq / resolution_hz) - 1;

    tim->hal_handle.Init.Prescaler = psc;
    tim->hal_handle.Init.CounterMode = TIM_COUNTERMODE_UP;
    tim->hal_handle.Init.Period = 0xFFFFFFFF;
    tim->hal_handle.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    tim->hal_handle.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;

    HAL_TIM_IC_Init(&tim->hal_handle);
    // Channel configuration is the caller's responsibility via tim_ic_config_channel()
}

void tim_ic_config_channel(Tim *tim, uint8_t channel, uint32_t polarity, uint8_t filter)
{
    if (channel < 1 || channel > 4)
    {
        return;
    }
    TIM_IC_InitTypeDef sConfigIC = {0};
    sConfigIC.ICPolarity = polarity;
    sConfigIC.ICSelection = TIM_ICSELECTION_DIRECTTI;
    sConfigIC.ICPrescaler = TIM_ICPSC_DIV1;
    sConfigIC.ICFilter = filter;
    HAL_TIM_IC_ConfigChannel(&tim->hal_handle, &sConfigIC, tim_channel(channel));
    HAL_TIM_IC_Start(&tim->hal_handle, tim_channel(channel));
}

uint32_t tim_ic_get_count(Tim *tim)
{
    return __HAL_TIM_GET_COUNTER(&tim->hal_handle);
}

uint32_t tim_ic_get_channel(Tim *tim, uint8_t channel)
{
    if (channel < 1 || channel > 4)
    {
        return 0;
    }
    return HAL_TIM_ReadCapturedValue(&tim->hal_handle, tim_channel(channel));
}

void tim_ic_enable_ch_irq(Tim *tim, uint8_t channel)
{
    uint32_t it_flag;
    switch (channel)
    {
        case 1:
            it_flag = TIM_IT_CC1;
            break;
        case 2:
            it_flag = TIM_IT_CC2;
            break;
        case 3:
            it_flag = TIM_IT_CC3;
            break;
        case 4:
            it_flag = TIM_IT_CC4;
            break;
        default:
            return;
    }
    __HAL_TIM_ENABLE_IT(&tim->hal_handle, it_flag);
}

void tim_ic_disable_ch_irq(Tim *tim, uint8_t channel)
{
    uint32_t it_flag;
    switch (channel)
    {
        case 1:
            it_flag = TIM_IT_CC1;
            break;
        case 2:
            it_flag = TIM_IT_CC2;
            break;
        case 3:
            it_flag = TIM_IT_CC3;
            break;
        case 4:
            it_flag = TIM_IT_CC4;
            break;
        default:
            return;
    }
    __HAL_TIM_DISABLE_IT(&tim->hal_handle, it_flag);
}

/* ── Base counter mode ────────────────────────────────────────────────────── */

void tim_base_init(Tim *tim, TIM_TypeDef *instance, uint32_t tick_hz)
{
    memset(tim, 0, sizeof(Tim));

    tim->hal_handle.Instance = instance;
    tim->freq_hz = tick_hz;

    tim_enable_clock(instance);

    uint32_t clk_freq = tim_get_clock_freq(instance);
    uint32_t psc = (clk_freq / tick_hz) - 1;

    tim->hal_handle.Init.Prescaler = psc;
    tim->hal_handle.Init.CounterMode = TIM_COUNTERMODE_UP;
    tim->hal_handle.Init.Period = 0xFFFF;
    tim->hal_handle.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    tim->hal_handle.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;

    HAL_TIM_Base_Init(&tim->hal_handle);
    HAL_TIM_Base_Start(&tim->hal_handle);
}

uint32_t tim_base_get_count(Tim *tim)
{
    return __HAL_TIM_GET_COUNTER(&tim->hal_handle);
}

void tim_base_start_it(Tim *tim)
{
    HAL_TIM_Base_Stop(&tim->hal_handle);
    tim_enable_nvic(tim->hal_handle.Instance);
    HAL_TIM_Base_Start_IT(&tim->hal_handle);
}

void tim_base_stop_it(Tim *tim)
{
    HAL_TIM_Base_Stop_IT(&tim->hal_handle);
}

/* ── Encoder mode ─────────────────────────────────────────────────────────── */

void tim_encoder_init(Tim *tim, TIM_TypeDef *instance, uint32_t mode)
{
    memset(tim, 0, sizeof(Tim));

    tim->hal_handle.Instance = instance;
    tim_enable_clock(instance);

    uint32_t max_count = (instance == TIM2) ? 0xFFFFFFFF : 0xFFFF;
    tim->hal_handle.Init.Prescaler = 0;
    tim->hal_handle.Init.CounterMode = TIM_COUNTERMODE_UP;
    tim->hal_handle.Init.Period = max_count;
    tim->hal_handle.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    tim->hal_handle.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;

    TIM_Encoder_InitTypeDef sEncoderConfig = {0};
    sEncoderConfig.EncoderMode = mode;
    sEncoderConfig.IC1Polarity = TIM_ICPOLARITY_RISING;
    sEncoderConfig.IC1Selection = TIM_ICSELECTION_DIRECTTI;
    sEncoderConfig.IC1Prescaler = TIM_ICPSC_DIV1;
    sEncoderConfig.IC1Filter = 0;
    sEncoderConfig.IC2Polarity = TIM_ICPOLARITY_RISING;
    sEncoderConfig.IC2Selection = TIM_ICSELECTION_DIRECTTI;
    sEncoderConfig.IC2Prescaler = TIM_ICPSC_DIV1;
    sEncoderConfig.IC2Filter = 0;

    HAL_TIM_Encoder_Init(&tim->hal_handle, &sEncoderConfig);
    HAL_TIM_Encoder_Start(&tim->hal_handle, TIM_CHANNEL_ALL);
}

uint32_t tim_encoder_get_count(Tim *tim)
{
    return __HAL_TIM_GET_COUNTER(&tim->hal_handle);
}

void tim_encoder_reset(Tim *tim)
{
    __HAL_TIM_SET_COUNTER(&tim->hal_handle, 0);
}

/* ── One-pulse mode ───────────────────────────────────────────────────────── */

void tim_one_pulse_init(
    Tim *tim, TIM_TypeDef *instance, uint8_t channel, uint32_t delay_us, uint32_t width_us)
{
    memset(tim, 0, sizeof(Tim));

    tim->hal_handle.Instance = instance;
    tim->op_delay_us = delay_us;
    tim->op_width_us = width_us;

    tim_enable_clock(instance);

    uint32_t clk_freq = tim_get_clock_freq(instance);
    uint32_t psc = (clk_freq / 1000000UL) - 1; // 1 µs tick at 48 MHz → PSC = 47

    tim->hal_handle.Init.Prescaler = psc;
    tim->hal_handle.Init.CounterMode = TIM_COUNTERMODE_UP;
    tim->hal_handle.Init.Period = delay_us + width_us - 1;
    tim->hal_handle.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    tim->hal_handle.Init.RepetitionCounter = 0;
    tim->hal_handle.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;

    HAL_TIM_OnePulse_Init(&tim->hal_handle, TIM_OPMODE_SINGLE);

    TIM_OnePulse_InitTypeDef sConfig = {0};
    sConfig.OCMode = TIM_OCMODE_PWM2;
    sConfig.OCPolarity = TIM_OCPOLARITY_HIGH;
    sConfig.OCNPolarity = TIM_OCNPOLARITY_HIGH;
    sConfig.OCIdleState = TIM_OCIDLESTATE_RESET;
    sConfig.OCNIdleState = TIM_OCNIDLESTATE_RESET;
    sConfig.Pulse = delay_us;
    sConfig.ICPolarity = TIM_ICPOLARITY_RISING;
    sConfig.ICSelection = TIM_ICSELECTION_DIRECTTI;
    sConfig.ICFilter = 0;

    HAL_TIM_OnePulse_ConfigChannel(
        &tim->hal_handle, &sConfig, tim_channel(channel), tim_channel(channel == 1 ? 2 : 1));
}

void tim_one_pulse_trigger(Tim *tim, uint8_t channel)
{
    HAL_TIM_OnePulse_Start(&tim->hal_handle, tim_channel(channel));
}

/* ── Output compare mode ──────────────────────────────────────────────────── */

void tim_oc_init(
    Tim *tim, TIM_TypeDef *instance, uint32_t tick_hz, uint8_t channel, uint32_t oc_mode)
{
    memset(tim, 0, sizeof(Tim));

    tim->hal_handle.Instance = instance;
    tim->freq_hz = tick_hz;

    tim_enable_clock(instance);

    uint32_t clk_freq = tim_get_clock_freq(instance);
    uint32_t psc = (clk_freq / tick_hz) - 1;
    uint32_t max_arr = (instance == TIM2) ? 0xFFFFFFFF : 0xFFFF;

    tim->hal_handle.Init.Prescaler = psc;
    tim->hal_handle.Init.CounterMode = TIM_COUNTERMODE_UP;
    tim->hal_handle.Init.Period = max_arr;
    tim->hal_handle.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    tim->hal_handle.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;

    HAL_TIM_OC_Init(&tim->hal_handle);

    TIM_OC_InitTypeDef sConfigOC = {0};
    sConfigOC.OCMode = oc_mode;
    sConfigOC.Pulse = 0;
    sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
    sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;

    HAL_TIM_OC_ConfigChannel(&tim->hal_handle, &sConfigOC, tim_channel(channel));
    HAL_TIM_OC_Start_IT(&tim->hal_handle, tim_channel(channel));
}

void tim_oc_set_compare(Tim *tim, uint8_t channel, uint32_t value)
{
    __HAL_TIM_SET_COMPARE(&tim->hal_handle, tim_channel(channel), value);
}

/* ── IRQ control ──────────────────────────────────────────────────────────── */

void tim_enable_irq(Tim *tim)
{
    tim_enable_nvic(tim->hal_handle.Instance);
    __HAL_TIM_ENABLE_IT(&tim->hal_handle, TIM_IT_UPDATE);
}

void tim_disable_irq(Tim *tim)
{
    __HAL_TIM_DISABLE_IT(&tim->hal_handle, TIM_IT_UPDATE);
}
