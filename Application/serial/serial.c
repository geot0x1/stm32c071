#include "serial.h"
#include "uart.h"
#include <stdarg.h>
#include <stdio.h>

#define SERIAL_PRINTF_BUF_SIZE   256U
#define SERIAL_PRINTF_TIMEOUT_MS 100U

static Uart_t serialUart;

static Serial_err_t map_err(Uart_err_t e)
{
    switch (e)
    {
        case UART_OK:          return SERIAL_OK;
        case UART_ERR_TIMEOUT: return SERIAL_ERR_TIMEOUT;
        case UART_ERR_BUSY:    return SERIAL_ERR_BUSY;
        default:               return SERIAL_ERR_HAL;
    }
}

void serial_init(USART_TypeDef *instance, uint32_t baud_rate)
{
    uart_init(&serialUart, instance, baud_rate);
}

Serial_err_t serial_write(const uint8_t *data, uint16_t len, uint32_t timeout_ms)
{
    return map_err(uart_write(&serialUart, data, len, timeout_ms));
}

Serial_err_t serial_read(uint8_t *data, uint16_t len, uint32_t timeout_ms)
{
    return map_err(uart_read(&serialUart, data, len, timeout_ms));
}

void serial_printf(const char *format, ...)
{
    char buf[SERIAL_PRINTF_BUF_SIZE];
    va_list args;
    va_start(args, format);
    int len = vsnprintf(buf, sizeof(buf), format, args);
    va_end(args);

    if (len > 0)
    {
        serial_write((const uint8_t *)buf, (uint16_t)len, SERIAL_PRINTF_TIMEOUT_MS);
    }
}
