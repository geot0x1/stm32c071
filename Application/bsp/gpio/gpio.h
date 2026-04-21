#ifndef BSP_GPIO_H
#define BSP_GPIO_H

#include <stdbool.h>
#include "stm32c0xx_hal.h"

typedef struct Gpio_s
{
    GPIO_TypeDef *port;
    uint16_t      pin;
} Gpio;

/**
 * @brief Initialize a push-pull output GPIO pin.
 *
 * @param gpio          Handle to fill
 * @param port          GPIO port (GPIOA, GPIOB, …)
 * @param pin           GPIO_PIN_x bitmask
 * @param initial_state GPIO_PIN_SET or GPIO_PIN_RESET
 */
void gpio_output_init(Gpio *gpio, GPIO_TypeDef *port, uint16_t pin,
                      GPIO_PinState initial_state);

/**
 * @brief Set a GPIO output pin state.
 *
 * @param gpio  Initialized output handle
 * @param state true → HIGH, false → LOW
 */
void gpio_write(Gpio *gpio, bool state);

/**
 * @brief Toggle a GPIO output pin.
 */
void gpio_toggle(Gpio *gpio);

/**
 * @brief Initialize an EXTI / interrupt input pin and enable its IRQ.
 *
 * @param gpio      Handle to fill
 * @param port      GPIO port
 * @param pin       GPIO_PIN_x bitmask
 * @param mode      GPIO_MODE_IT_RISING, GPIO_MODE_IT_FALLING, etc.
 * @param pull      GPIO_PULLUP, GPIO_PULLDOWN, or GPIO_NOPULL
 * @param irqn      IRQ number (e.g. EXTI4_15_IRQn)
 * @param priority  NVIC priority level
 */
void gpio_exti_init(Gpio *gpio, GPIO_TypeDef *port, uint16_t pin,
                    uint32_t mode, uint32_t pull,
                    IRQn_Type irqn, uint32_t priority);

/**
 * @brief Initialize a pin as open-drain output with pull-up (1-Wire idle state).
 *
 * The BSP sets the pin mode once and drives it high (released).
 * The caller owns all runtime register writes after this call.
 *
 * @param gpio  Handle to fill
 * @param port  GPIO port
 * @param pin   GPIO_PIN_x bitmask
 */
void gpio_open_drain_init(Gpio *gpio, GPIO_TypeDef *port, uint16_t pin);

#endif /* BSP_GPIO_H */
