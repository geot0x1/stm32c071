#include "board.h"
#include "board_config.h"
#include "stm32c0xx_hal.h"
#include "gpio.h"

/* ── Private peripheral instances ─────────────────────────────────────────── */

static Gpio_t led_gpio;
static Gpio_t exti_gpio;
static Gpio_t onewire_gpio;

static I2c_t  sensor_i2c;
static Usb_t  board_usb;

/* ── Forward declarations ──────────────────────────────────────────────────── */

static void SystemClock_Config(void);
static void MX_FLASH_Init(void);

/* ── Public API ────────────────────────────────────────────────────────────── */

void board_init(void)
{
    /* 1. System clock (must be first) */
    SystemClock_Config();

    /* 2. GPIO — enable all port clocks once so BSP helpers are idempotent */
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_GPIOF_CLK_ENABLE();

    /* 2a. Status LED (output, starts LOW) */
    gpio_output_init(&led_gpio, BOARD_LED_PORT, BOARD_LED_PIN, GPIO_PIN_RESET);

    /* 2b. External interrupt input */
    gpio_exti_init(&exti_gpio, BOARD_EXTI_PORT, BOARD_EXTI_PIN,
                   BOARD_EXTI_MODE, BOARD_EXTI_PULL,
                   BOARD_EXTI_IRQn, 0);

    /* 2c. 1-Wire bus (open-drain + pull-up; runtime owned by onewire.c) */
    gpio_open_drain_init(&onewire_gpio, BOARD_ONEWIRE_PORT, BOARD_ONEWIRE_PIN);

    /* 3. I2C sensor bus (GPIO AF handled by MspInit) */
    i2c_init(&sensor_i2c, BOARD_I2C_INSTANCE, BOARD_I2C_TIMING);

    /* 4. USB PCD — HAL_PCD_MspInit configures USB clock source + enable */
    usb_pcd_init(&board_usb);
    HAL_Delay(20); /* USB clock stabilization */

    /* 5. Flash — verify access (required before NVS use) */
    MX_FLASH_Init();
}

/* ── LED control ───────────────────────────────────────────────────────────── */

void board_led_set(bool on)
{
    gpio_write(&led_gpio, on);
}

void board_led_toggle(void)
{
    gpio_toggle(&led_gpio);
}

/* ── Peripheral getters ────────────────────────────────────────────────────── */

I2c_t *board_get_i2c(void)
{
    return &sensor_i2c;
}

Usb_t *board_get_usb(void)
{
    return &board_usb;
}

/* ── Private init helpers ──────────────────────────────────────────────────── */

/**
 * @brief System Clock Configuration
 *
 * System Clock source    = HSE (8 MHz external crystal)
 * SYSCLK / HCLK / APB1  = 48 MHz (no divisors)
 * HSI48                  = ON  (required for USB)
 */
static void SystemClock_Config(void)
{
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

    __HAL_FLASH_SET_LATENCY(FLASH_LATENCY_1);

    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE | RCC_OSCILLATORTYPE_HSI48;
    RCC_OscInitStruct.HSEState       = RCC_HSE_ON;
    RCC_OscInitStruct.HSI48State     = RCC_HSI48_ON;

    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
    {
        Error_Handler();
    }

    RCC_ClkInitStruct.ClockType      = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK
                                     | RCC_CLOCKTYPE_PCLK1;
    RCC_ClkInitStruct.SYSCLKSource   = RCC_SYSCLKSOURCE_HSE;
    RCC_ClkInitStruct.SYSCLKDivider  = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.AHBCLKDivider  = RCC_HCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_APB1_DIV1;

    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_1) != HAL_OK)
    {
        Error_Handler();
    }
}

/**
 * @brief Verify FLASH access (unlock/lock cycle — required before NVS use).
 */
static void MX_FLASH_Init(void)
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

/* ── Error handling ────────────────────────────────────────────────────────── */

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
