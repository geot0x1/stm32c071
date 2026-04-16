#include "board.h"
#include "board_config.h"
#include "gpio.h"
#include "uart.h"
#include "stm32c0xx_hal.h"

/* ── Private peripheral instances ───────────────────────────────────────────
 */

static Gpio_t led_gpio;
static Gpio_t exti_gpio;
static Gpio_t onewire_gpio;
static Gpio_t onewire_pwr_en_gpio;
static Gpio_t onewire_pu_en_gpio;
static Gpio_t lcd_pwr_en_gpio;

static I2c_t sensor_i2c;
static Usb_t board_usb;

static Uart_t board_uart1;

/* ── Forward declarations ────────────────────────────────────────────────────
 */

static void system_clock_config(void);
static void mx_flash_init(void);

/* ── Public API ──────────────────────────────────────────────────────────────
 */

void board_init(void)
{
    /* 1. System clock (must be first) */
    system_clock_config();

    /* 2. GPIO — enable all port clocks once so BSP helpers are idempotent */
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_GPIOF_CLK_ENABLE();

    /* 2a. Status LED (output, starts LOW) */
    gpio_output_init(&led_gpio, BOARD_LED_PORT, BOARD_LED_PIN, GPIO_PIN_RESET);

    /* 2b. External interrupt input */
    gpio_exti_init(&exti_gpio, BOARD_EXTI_PORT, BOARD_EXTI_PIN, BOARD_EXTI_MODE,
                   BOARD_EXTI_PULL, BOARD_EXTI_IRQn, 0);

    /* 2c. 1-Wire bus (open-drain + pull-up; runtime owned by onewire.c) */
    gpio_open_drain_init(&onewire_gpio, BOARD_ONEWIRE_PORT, BOARD_ONEWIRE_PIN);

    /* 2d. 1-Wire power supply enable — start OFF until bus is ready */
    gpio_output_init(&onewire_pwr_en_gpio, BOARD_ONEWIRE_PWR_EN_PORT,
                     BOARD_ONEWIRE_PWR_EN_PIN, GPIO_PIN_RESET);

    /* 2e. 1-Wire strong pull-up enable — start OFF */
    gpio_output_init(&onewire_pu_en_gpio, BOARD_ONEWIRE_PU_EN_PORT,
                     BOARD_ONEWIRE_PU_EN_PIN, GPIO_PIN_RESET);

    /* 2f. LCD power enable — start OFF */
    gpio_output_init(&lcd_pwr_en_gpio, BOARD_LCD_PWR_EN_PORT,
                     BOARD_LCD_PWR_EN_PIN, GPIO_PIN_RESET);

    /* 3. I2C sensor bus (GPIO AF handled by MspInit) */
    i2c_init(&sensor_i2c, BOARD_I2C_INSTANCE, BOARD_I2C_TIMING);

    /* 4. USB PCD — HAL_PCD_MspInit configures USB clock source + enable */
    usb_pcd_init(&board_usb);
    HAL_Delay(20); /* USB clock stabilization */

    /* 5. UART1 (debug serial, PB6/PB7) — MSP init handled by HAL */
    uart_init(&board_uart1, BOARD_UART1_INSTANCE, BOARD_UART1_BAUD_RATE);

    /* 6. Flash — verify access (required before NVS use) */
    mx_flash_init();
}

/* ── LED control ─────────────────────────────────────────────────────────────
 */

void board_led_set(bool on)
{
    gpio_write(&led_gpio, on);
}

void board_led_toggle(void)
{
    gpio_toggle(&led_gpio);
}

/* ── Power enable outputs ────────────────────────────────────────────────────
 */

void board_onewire_power_set(bool on)
{
    gpio_write(&onewire_pwr_en_gpio, on);
}

void board_onewire_pullup_set(bool on)
{
    gpio_write(&onewire_pu_en_gpio, on);
}

void board_lcd_power_set(bool on)
{
    gpio_write(&lcd_pwr_en_gpio, on);
}

/* ── Peripheral getters ──────────────────────────────────────────────────────
 */

I2c_t *board_get_i2c(void)
{
    return &sensor_i2c;
}

Usb_t *board_get_usb(void)
{
    return &board_usb;
}

Uart_t *board_get_uart(void)
{
    return &board_uart1;
}

/* ── Private init helpers ────────────────────────────────────────────────────
 */

/**
 * @brief System Clock Configuration
 *
 * System Clock source    = HSE (8 MHz external crystal)
 * SYSCLK / HCLK / APB1  = 48 MHz (no divisors)
 * HSI48                  = ON  (required for USB)
 */
static void system_clock_config(void)
{
    RCC_OscInitTypeDef rcc_osc_init = {0};
    RCC_ClkInitTypeDef rcc_clk_init = {0};

    __HAL_FLASH_SET_LATENCY(FLASH_LATENCY_1);

    rcc_osc_init.OscillatorType =
        RCC_OSCILLATORTYPE_HSE | RCC_OSCILLATORTYPE_HSI48 | RCC_OSCILLATORTYPE_LSI;
    rcc_osc_init.HSEState = RCC_HSE_ON;
    rcc_osc_init.HSI48State = RCC_HSI48_ON;
    rcc_osc_init.LSIState = RCC_LSI_ON;

    if (HAL_RCC_OscConfig(&rcc_osc_init) != HAL_OK)
    {
        Error_Handler();
    }

    rcc_clk_init.ClockType =
        RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_PCLK1;
    rcc_clk_init.SYSCLKSource = RCC_SYSCLKSOURCE_HSE;
    rcc_clk_init.SYSCLKDivider = RCC_SYSCLK_DIV1;
    rcc_clk_init.AHBCLKDivider = RCC_HCLK_DIV1;
    rcc_clk_init.APB1CLKDivider = RCC_APB1_DIV1;

    if (HAL_RCC_ClockConfig(&rcc_clk_init, FLASH_LATENCY_1) != HAL_OK)
    {
        Error_Handler();
    }
}

/**
 * @brief Verify FLASH access (unlock/lock cycle — required before NVS use).
 */
static void mx_flash_init(void)
{
    if (HAL_FLASH_Unlock() != HAL_OK)
    {
        Error_Handler();
    }

    if (HAL_FLASH_Lock() != HAL_OK)
    {
        Error_Handler();
    }
}

/* ── Error handling ──────────────────────────────────────────────────────────
 */

void Error_Handler(void)
{
    __disable_irq();
    while (1)
    {
    }
}

#ifdef USE_FULL_ASSERT
void assert_failed(uint8_t *file, uint32_t line)
{
    (void)file;
    (void)line;
}
#endif
