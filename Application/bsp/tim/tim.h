#ifndef BSP_TIM_H
#define BSP_TIM_H

#include <stdint.h>
#include <stdbool.h>
#include "stm32c0xx_hal.h"

/**
 * @brief Opaque timer handle
 *
 * Clients allocate a Tim struct (typically static) and pass it to
 * tim_*_init(). The handle encapsulates the HAL state and peripheral config.
 */
typedef struct TimS {
    TIM_HandleTypeDef hal_handle;
    uint32_t freq_hz;
    uint8_t num_channels;
    uint32_t op_delay_us;   // one-pulse mode: delay before pulse leading edge (µs)
    uint32_t op_width_us;   // one-pulse mode: pulse width (µs)
} Tim;

/* ── PWM output mode ──────────────────────────────────────────────────────── */

/**
 * @brief Initialize a timer as a PWM output
 *
 * @param tim           Timer handle (allocated by caller)
 * @param instance      TIM peripheral instance (TIM1, TIM3, TIM16, TIM17, etc.)
 * @param freq_hz       PWM frequency in Hz
 * @param num_channels  Number of active channels (1-4)
 */
void tim_pwm_init(Tim *tim, TIM_TypeDef *instance, uint32_t freq_hz, uint8_t num_channels);

/**
 * @brief Set PWM duty cycle on a channel (0-100%)
 *
 * @param tim       Timer handle
 * @param channel   Channel index (1-4)
 * @param duty_pct  Duty cycle (0-100%)
 */
void tim_pwm_set_duty(Tim *tim, uint8_t channel, uint8_t duty_pct);

/**
 * @brief Read back the current PWM duty cycle on a channel
 *
 * @param tim       Timer handle
 * @param channel   Channel index (1-4)
 * @return          Duty cycle 0-100%, or 0 on invalid channel
 */
uint8_t tim_pwm_get_duty(Tim *tim, uint8_t channel);

/**
 * @brief Start PWM output on a channel
 *
 * @param tim       Timer handle
 * @param channel   Channel index (1-4)
 */
void tim_pwm_start(Tim *tim, uint8_t channel);

/**
 * @brief Stop PWM output on a channel
 *
 * @param tim       Timer handle
 * @param channel   Channel index (1-4)
 */
void tim_pwm_stop(Tim *tim, uint8_t channel);

/**
 * @brief Set PWM frequency (updates ARR/PSC)
 *
 * @param tim       Timer handle
 * @param freq_hz   New frequency in Hz
 */
void tim_pwm_set_freq(Tim *tim, uint32_t freq_hz);

/**
 * @brief Start complementary PWM output (CHxN) on an advanced timer channel
 *
 * @param tim       Timer handle (must be TIM1, TIM16, or TIM17)
 * @param channel   Channel index (1-3 for TIM1; 1 only for TIM16/TIM17)
 *
 * @note GPIO for CHxN must be configured in HAL_TIM_MspPostInit before calling.
 *       Dead-time should be set with tim_pwm_set_deadtime() before enabling
 *       complementary output if shoot-through protection is needed.
 */
void tim_pwm_start_n(Tim *tim, uint8_t channel);

/**
 * @brief Stop complementary PWM output (CHxN)
 *
 * @param tim       Timer handle
 * @param channel   Channel index
 */
void tim_pwm_stop_n(Tim *tim, uint8_t channel);

/**
 * @brief Set complementary PWM dead-time for an advanced timer
 *
 * @param tim           Timer handle (TIM1, TIM16, or TIM17 only)
 * @param deadtime_ns   Dead-time in nanoseconds
 *
 * @note At 48 MHz PCLK (t_DTS ≈ 20.8 ns with CKD=DIV1), linear range
 *       DTG[6:0] covers 0–127 steps (max ~2.6 µs). Values are clamped to
 *       this linear range. Call after tim_pwm_init().
 */
void tim_pwm_set_deadtime(Tim *tim, uint32_t deadtime_ns);

/* ── Input capture mode ───────────────────────────────────────────────────── */

/**
 * @brief Initialize a timer for input capture (base only — no channels started)
 *
 * Sets up the timer base (PSC/ARR) for the desired counting resolution and
 * calls HAL_TIM_IC_Init(). Channel configuration and start must be done
 * separately via tim_ic_config_channel().
 *
 * @param tim           Timer handle
 * @param instance      TIM peripheral instance (TIM2, TIM3, etc.)
 * @param resolution_hz Counting frequency (e.g., 1000000 for 1 µs resolution)
 */
void tim_ic_init(Tim *tim, TIM_TypeDef *instance, uint32_t resolution_hz);

/**
 * @brief Configure and start a single input capture channel
 *
 * @param tim       Timer handle (must have been initialized with tim_ic_init)
 * @param channel   Channel index (1-4)
 * @param polarity  TIM_ICPOLARITY_RISING, TIM_ICPOLARITY_FALLING, or
 *                  TIM_ICPOLARITY_BOTHEDGE
 * @param filter    Digital noise filter (0-15; 0 = no filter)
 */
void tim_ic_config_channel(Tim *tim, uint8_t channel, uint32_t polarity, uint8_t filter);

/**
 * @brief Get the current counter value of an input capture timer
 *
 * @param tim       Timer handle
 * @return          Counter value
 */
uint32_t tim_ic_get_count(Tim *tim);

/**
 * @brief Get capture value on a channel
 *
 * @param tim       Timer handle
 * @param channel   Channel index (1-4)
 * @return          Captured value
 */
uint32_t tim_ic_get_channel(Tim *tim, uint8_t channel);

/**
 * @brief Enable capture/compare interrupt for a specific input capture channel
 *
 * @param tim       Timer handle
 * @param channel   Channel index (1-4)
 *
 * @note The NVIC line must already be enabled (done automatically by
 *       HAL_TIM_Base_MspInit / HAL_TIM_IC_Init MSP callbacks).
 *       This function only enables the per-channel TIM_IT_CCx flag.
 */
void tim_ic_enable_ch_irq(Tim *tim, uint8_t channel);

/**
 * @brief Disable capture/compare interrupt for a specific input capture channel
 *
 * @param tim       Timer handle
 * @param channel   Channel index (1-4)
 */
void tim_ic_disable_ch_irq(Tim *tim, uint8_t channel);

/* ── Base counter mode ────────────────────────────────────────────────────── */

/**
 * @brief Initialize a timer as a free-running base counter (polling mode)
 *
 * @param tim       Timer handle
 * @param instance  TIM peripheral instance (TIM14, etc.)
 * @param tick_hz   Counting frequency in Hz
 */
void tim_base_init(Tim *tim, TIM_TypeDef *instance, uint32_t tick_hz);

/**
 * @brief Get the current counter value of a base timer
 *
 * @param tim       Timer handle
 * @return          Counter value
 */
uint32_t tim_base_get_count(Tim *tim);

/**
 * @brief Start base timer with update (overflow) interrupt enabled
 *
 * Stops any running polling-mode counter, enables NVIC, and restarts
 * the timer in interrupt mode. HAL_TIM_PeriodElapsedCallback() is called
 * on each overflow.
 *
 * @param tim       Timer handle (must have been initialized with tim_base_init)
 */
void tim_base_start_it(Tim *tim);

/**
 * @brief Stop base timer and disable update interrupt
 *
 * @param tim       Timer handle
 */
void tim_base_stop_it(Tim *tim);

/* ── Encoder mode ─────────────────────────────────────────────────────────── */

/**
 * @brief Initialize a timer in quadrature encoder mode
 *
 * Configures CH1 and CH2 as encoder inputs (no filter, no prescaler).
 * Counter runs between 0 and ARR (0xFFFFFFFF for TIM2, 0xFFFF for others).
 * Counting direction is automatic based on quadrature phase.
 *
 * @param tim       Timer handle
 * @param instance  TIM peripheral instance (TIM2 or TIM3 — both support encoder)
 * @param mode      TIM_ENCODERMODE_TI1, TIM_ENCODERMODE_TI2, or
 *                  TIM_ENCODERMODE_TI12 (x4 quadrature)
 *
 * @note GPIO for encoder input pins must be configured before calling this
 *       (via HAL_TIM_MspPostInit or inline GPIO init).
 *       TIM2 and TIM3 are currently assigned to IC and PWM respectively —
 *       use encoder mode only when repurposing one of these timers.
 */
void tim_encoder_init(Tim *tim, TIM_TypeDef *instance, uint32_t mode);

/**
 * @brief Read the current encoder counter value
 *
 * @param tim       Timer handle
 * @return          Counter value (32-bit for TIM2, 16-bit for TIM3)
 */
uint32_t tim_encoder_get_count(Tim *tim);

/**
 * @brief Reset encoder counter to zero
 *
 * @param tim       Timer handle
 */
void tim_encoder_reset(Tim *tim);

/* ── One-pulse mode ───────────────────────────────────────────────────────── */

/**
 * @brief Initialize a timer in one-pulse (single-shot) mode
 *
 * Generates a precise single output pulse of configurable delay and width.
 * PSC is set for 1 µs resolution (at 48 MHz PCLK: PSC = 47).
 * ARR = delay_us + width_us - 1; CCR = delay_us.
 *
 * @param tim       Timer handle
 * @param instance  TIM peripheral instance
 * @param channel   Output channel (1-4; 1 only for TIM14/TIM16/TIM17)
 * @param delay_us  Delay from trigger to pulse leading edge (µs)
 * @param width_us  Pulse width (µs)
 *
 * @note GPIO for the output channel must be configured before calling.
 */
void tim_one_pulse_init(Tim *tim, TIM_TypeDef *instance, uint8_t channel,
                        uint32_t delay_us, uint32_t width_us);

/**
 * @brief Trigger a one-pulse output (software trigger)
 *
 * Arms and starts the one-pulse counter. The output fires once and stops.
 *
 * @param tim       Timer handle
 * @param channel   Channel index (1-4)
 */
void tim_one_pulse_trigger(Tim *tim, uint8_t channel);

/* ── Output compare mode ──────────────────────────────────────────────────── */

/**
 * @brief Initialize a timer channel in Output Compare mode
 *
 * Configures the timer for compare-match events. Starts counting and
 * enables the capture/compare interrupt for the channel.
 *
 * @param tim       Timer handle
 * @param instance  TIM peripheral instance
 * @param tick_hz   Counter frequency in Hz
 * @param channel   Channel index (1-4)
 * @param oc_mode   HAL OC mode constant:
 *                    TIM_OCMODE_TIMING    — interrupt only, no pin toggle
 *                    TIM_OCMODE_TOGGLE    — pin toggles on match
 *                    TIM_OCMODE_ACTIVE    — pin set high on match
 *                    TIM_OCMODE_INACTIVE  — pin set low on match
 *
 * @note For pin output modes (TOGGLE/ACTIVE/INACTIVE), GPIO for the channel
 *       must be configured in HAL_TIM_MspPostInit or HAL_TIM_OC_MspInit.
 */
void tim_oc_init(Tim *tim, TIM_TypeDef *instance, uint32_t tick_hz,
                 uint8_t channel, uint32_t oc_mode);

/**
 * @brief Update the compare value for an output compare channel
 *
 * @param tim       Timer handle
 * @param channel   Channel index (1-4)
 * @param value     New compare register value (in counter ticks)
 */
void tim_oc_set_compare(Tim *tim, uint8_t channel, uint32_t value);

/* ── IRQ control ──────────────────────────────────────────────────────────── */

/**
 * @brief Enable timer update (overflow) interrupt
 *
 * @param tim       Timer handle
 */
void tim_enable_irq(Tim *tim);

/**
 * @brief Disable timer update (overflow) interrupt
 *
 * @param tim       Timer handle
 */
void tim_disable_irq(Tim *tim);

#endif // BSP_TIM_H
