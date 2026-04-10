#include "tim.h"
#include <string.h>

/**
 * @brief Get APBx clock frequency for a given timer instance
 *
 * Note: STM32C0 has only APB1. All timers run on PCLK1.
 */
static uint32_t tim_get_clock_freq(TIM_TypeDef *instance) {
    // All timers in STM32C0 run on APB1 (PCLK1)
    return HAL_RCC_GetPCLK1Freq();
}

/**
 * @brief Enable peripheral clock for timer
 */
static void tim_enable_clock(TIM_TypeDef *instance) {
    if (instance == TIM1) {
        __HAL_RCC_TIM1_CLK_ENABLE();
    } else if (instance == TIM2) {
        __HAL_RCC_TIM2_CLK_ENABLE();
    } else if (instance == TIM3) {
        __HAL_RCC_TIM3_CLK_ENABLE();
    } else if (instance == TIM14) {
        __HAL_RCC_TIM14_CLK_ENABLE();
    } else if (instance == TIM16) {
        __HAL_RCC_TIM16_CLK_ENABLE();
    } else if (instance == TIM17) {
        __HAL_RCC_TIM17_CLK_ENABLE();
    }
}

/**
 * @brief Enable NVIC interrupt for timer
 */
static void tim_enable_nvic(TIM_TypeDef *instance) {
    if (instance == TIM1) {
        HAL_NVIC_EnableIRQ(TIM1_BRK_UP_TRG_COM_IRQn);
    } else if (instance == TIM2) {
        HAL_NVIC_EnableIRQ(TIM2_IRQn);
    } else if (instance == TIM3) {
        HAL_NVIC_EnableIRQ(TIM3_IRQn);
    } else if (instance == TIM14) {
        HAL_NVIC_EnableIRQ(TIM14_IRQn);
    } else if (instance == TIM16) {
        HAL_NVIC_EnableIRQ(TIM16_IRQn);
    } else if (instance == TIM17) {
        HAL_NVIC_EnableIRQ(TIM17_IRQn);
    }
}

/**
 * @brief Initialize PWM timer
 */
void tim_pwm_init(Tim_t *tim, TIM_TypeDef *instance, uint32_t freq_hz, uint8_t num_channels) {
    memset(tim, 0, sizeof(Tim_t));

    tim->hal_handle.Instance = instance;
    tim->freq_hz = freq_hz;
    tim->num_channels = num_channels;

    // Enable peripheral clock
    tim_enable_clock(instance);

    // Calculate prescaler and ARR for desired frequency
    uint32_t clk_freq = tim_get_clock_freq(instance);
    uint32_t arr = clk_freq / freq_hz;

    tim->hal_handle.Init.Prescaler = 0;
    tim->hal_handle.Init.CounterMode = TIM_COUNTERMODE_UP;
    tim->hal_handle.Init.Period = (arr > 0xFFFF) ? 0xFFFF : (arr - 1);
    tim->hal_handle.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    tim->hal_handle.Init.RepetitionCounter = 0;
    tim->hal_handle.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;

    // Initialize base timer
    HAL_TIM_PWM_Init(&tim->hal_handle);

    // Configure all channels
    TIM_OC_InitTypeDef sConfigOC = {0};
    sConfigOC.OCMode = TIM_OCMODE_PWM1;
    sConfigOC.Pulse = 0;
    sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
    sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;

    for (uint8_t ch = 1; ch <= num_channels; ch++) {
        HAL_TIM_PWM_ConfigChannel(&tim->hal_handle, &sConfigOC, (ch - 1) << 2);
    }

    // Configure BreakDeadTime for advanced timers (TIM1, TIM16, TIM17)
    if (instance == TIM1 || instance == TIM16 || instance == TIM17) {
        TIM_BreakDeadTimeConfigTypeDef sBreakDeadTimeConfig = {0};
        sBreakDeadTimeConfig.OffStateRunMode = TIM_OSSR_DISABLE;
        sBreakDeadTimeConfig.OffStateIDLEMode = TIM_OSSI_DISABLE;
        sBreakDeadTimeConfig.LockLevel = TIM_LOCKLEVEL_OFF;
        sBreakDeadTimeConfig.DeadTime = 0;
        sBreakDeadTimeConfig.BreakState = TIM_BREAK_DISABLE;
        sBreakDeadTimeConfig.BreakPolarity = TIM_BREAKPOLARITY_HIGH;
        sBreakDeadTimeConfig.BreakFilter = 0;
        sBreakDeadTimeConfig.AutomaticOutput = TIM_AUTOMATICOUTPUT_DISABLE;
        if (instance == TIM1) {
            sBreakDeadTimeConfig.Break2State = TIM_BREAK2_DISABLE;
            sBreakDeadTimeConfig.Break2Polarity = TIM_BREAK2POLARITY_HIGH;
            sBreakDeadTimeConfig.Break2Filter = 0;
            sBreakDeadTimeConfig.BreakAFMode = TIM_BREAK_AFMODE_INPUT;
            HAL_TIMEx_ConfigBreakDeadTime(&tim->hal_handle, &sBreakDeadTimeConfig);
        } else {
            HAL_TIMEx_ConfigBreakDeadTime(&tim->hal_handle, &sBreakDeadTimeConfig);
        }
    }
}

/**
 * @brief Set PWM duty on a channel
 */
void tim_pwm_set_duty(Tim_t *tim, uint8_t channel, uint8_t duty_pct) {
    if (channel < 1 || channel > tim->num_channels || duty_pct > 100) {
        return;
    }

    uint32_t pulse = (tim->hal_handle.Init.Period + 1) * duty_pct / 100;
    __HAL_TIM_SET_COMPARE(&tim->hal_handle, (channel - 1) << 2, pulse);
}

/**
 * @brief Start PWM on a channel
 */
void tim_pwm_start(Tim_t *tim, uint8_t channel) {
    if (channel < 1 || channel > tim->num_channels) {
        return;
    }
    HAL_TIM_PWM_Start(&tim->hal_handle, (channel - 1) << 2);
}

/**
 * @brief Stop PWM on a channel
 */
void tim_pwm_stop(Tim_t *tim, uint8_t channel) {
    if (channel < 1 || channel > tim->num_channels) {
        return;
    }
    HAL_TIM_PWM_Stop(&tim->hal_handle, (channel - 1) << 2);
}

/**
 * @brief Update PWM frequency
 */
void tim_pwm_set_freq(Tim_t *tim, uint32_t freq_hz) {
    tim->freq_hz = freq_hz;
    uint32_t clk_freq = tim_get_clock_freq(tim->hal_handle.Instance);
    uint32_t arr = (clk_freq / freq_hz) - 1;

    __HAL_TIM_SET_AUTORELOAD(&tim->hal_handle, arr);
    __HAL_TIM_CLEAR_FLAG(&tim->hal_handle, TIM_FLAG_UPDATE);
}

/**
 * @brief Initialize input capture timer
 */
void tim_ic_init(Tim_t *tim, TIM_TypeDef *instance, uint32_t resolution_hz) {
    memset(tim, 0, sizeof(Tim_t));

    tim->hal_handle.Instance = instance;
    tim->freq_hz = resolution_hz;

    tim_enable_clock(instance);

    // Calculate prescaler for desired resolution
    uint32_t clk_freq = tim_get_clock_freq(instance);
    uint32_t psc = (clk_freq / resolution_hz) - 1;

    tim->hal_handle.Init.Prescaler = psc;
    tim->hal_handle.Init.CounterMode = TIM_COUNTERMODE_UP;
    tim->hal_handle.Init.Period = 0xFFFFFFFF;  // 32-bit counter
    tim->hal_handle.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    tim->hal_handle.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;

    HAL_TIM_IC_Init(&tim->hal_handle);

    // Configure input capture on channels 3 and 4 (both-edge capture)
    TIM_IC_InitTypeDef sConfigIC = {0};
    sConfigIC.ICPolarity = TIM_INPUTCHANNELPOLARITY_RISING;
    sConfigIC.ICSelection = TIM_ICSELECTION_DIRECTTI;
    sConfigIC.ICPrescaler = TIM_ICPSC_DIV1;
    sConfigIC.ICFilter = 0;

    HAL_TIM_IC_ConfigChannel(&tim->hal_handle, &sConfigIC, TIM_CHANNEL_3);
    HAL_TIM_IC_ConfigChannel(&tim->hal_handle, &sConfigIC, TIM_CHANNEL_4);

    // Start input capture
    HAL_TIM_IC_Start(&tim->hal_handle, TIM_CHANNEL_3);
    HAL_TIM_IC_Start(&tim->hal_handle, TIM_CHANNEL_4);
}

/**
 * @brief Get counter value from input capture timer
 */
uint32_t tim_ic_get_count(Tim_t *tim) {
    return __HAL_TIM_GET_COUNTER(&tim->hal_handle);
}

/**
 * @brief Get capture value from input capture channel
 */
uint32_t tim_ic_get_channel(Tim_t *tim, uint8_t channel) {
    if (channel < 1 || channel > 4) {
        return 0;
    }
    return HAL_TIM_ReadCapturedValue(&tim->hal_handle, (channel - 1) << 2);
}

/**
 * @brief Initialize base timer
 */
void tim_base_init(Tim_t *tim, TIM_TypeDef *instance, uint32_t tick_hz) {
    memset(tim, 0, sizeof(Tim_t));

    tim->hal_handle.Instance = instance;
    tim->freq_hz = tick_hz;

    tim_enable_clock(instance);

    // Calculate prescaler
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

/**
 * @brief Get counter value from base timer
 */
uint32_t tim_base_get_count(Tim_t *tim) {
    return __HAL_TIM_GET_COUNTER(&tim->hal_handle);
}

/**
 * @brief Enable timer interrupt
 */
void tim_enable_irq(Tim_t *tim) {
    tim_enable_nvic(tim->hal_handle.Instance);
    __HAL_TIM_ENABLE_IT(&tim->hal_handle, TIM_IT_UPDATE);
}

/**
 * @brief Disable timer interrupt
 */
void tim_disable_irq(Tim_t *tim) {
    __HAL_TIM_DISABLE_IT(&tim->hal_handle, TIM_IT_UPDATE);
}
