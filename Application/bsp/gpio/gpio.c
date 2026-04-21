#include "gpio.h"

static void gpio_enable_clock(GPIO_TypeDef *port)
{
    if (port == GPIOA)
    {
        __HAL_RCC_GPIOA_CLK_ENABLE();
    }
    else if (port == GPIOB)
    {
        __HAL_RCC_GPIOB_CLK_ENABLE();
    }
    else if (port == GPIOC)
    {
        __HAL_RCC_GPIOC_CLK_ENABLE();
    }
    else if (port == GPIOF)
    {
        __HAL_RCC_GPIOF_CLK_ENABLE();
    }
}

void gpio_output_init(Gpio *gpio, GPIO_TypeDef *port, uint16_t pin,
                      GPIO_PinState initial_state)
{
    gpio->port = port;
    gpio->pin  = pin;

    gpio_enable_clock(port);
    HAL_GPIO_WritePin(port, pin, initial_state);

    GPIO_InitTypeDef init = {0};
    init.Pin   = pin;
    init.Mode  = GPIO_MODE_OUTPUT_PP;
    init.Pull  = GPIO_NOPULL;
    init.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(port, &init);
}

void gpio_write(Gpio *gpio, bool state)
{
    HAL_GPIO_WritePin(gpio->port, gpio->pin,
                      state ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

void gpio_toggle(Gpio *gpio)
{
    HAL_GPIO_TogglePin(gpio->port, gpio->pin);
}

void gpio_exti_init(Gpio *gpio, GPIO_TypeDef *port, uint16_t pin,
                    uint32_t mode, uint32_t pull,
                    IRQn_Type irqn, uint32_t priority)
{
    gpio->port = port;
    gpio->pin  = pin;

    gpio_enable_clock(port);

    GPIO_InitTypeDef init = {0};
    init.Pin  = pin;
    init.Mode = mode;
    init.Pull = pull;
    HAL_GPIO_Init(port, &init);

    HAL_NVIC_SetPriority(irqn, priority, 0);
    HAL_NVIC_EnableIRQ(irqn);
}

void gpio_open_drain_init(Gpio *gpio, GPIO_TypeDef *port, uint16_t pin)
{
    gpio->port = port;
    gpio->pin  = pin;

    gpio_enable_clock(port);

    GPIO_InitTypeDef init = {0};
    init.Pin   = pin;
    init.Mode  = GPIO_MODE_OUTPUT_OD;
    init.Pull  = GPIO_PULLUP;
    init.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(port, &init);

    /* Drive high (released / idle-high for 1-Wire) */
    HAL_GPIO_WritePin(port, pin, GPIO_PIN_SET);
}
