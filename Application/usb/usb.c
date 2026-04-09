#include "usb.h"
#include "tusb.h"
#include <stdio.h>
#include <stdarg.h>

/**
 * @brief Initialize the USB module.
 */
void usb_init(void)
{
    // TinyUSB initialization (tud_init) is called here.
    // Note: The hardware-specific clock initialization is handled in main.c.
    tusb_init();
}

/**
 * @brief Periodic tasks to handle USB communication.
 * Should be called in the main loop.
 */
void usb_task(void)
{
    tud_task();
}

/**
 * @brief Send raw data over the USB CDC interface.
 * 
 * @param buffer Pointer to the data to send
 * @param size Number of bytes to send
 * @return uint32_t Number of bytes successfully sent
 */
uint32_t usb_write(const void * buffer, uint32_t size)
{
    uint32_t totalSent = 0;
    
    // Check if the FIFO has enough space
    uint32_t available = tud_cdc_write_available();
    
    if (available > 0)
    {
        totalSent = tud_cdc_write(buffer, size);
        tud_cdc_write_flush();
    }
    
    return totalSent;
}

/**
 * @brief Read raw data from the USB CDC interface.
 * 
 * @param buffer Pointer to the buffer to store received data
 * @param size Maximum number of bytes to read
 * @return uint32_t Number of bytes successfully read
 */
uint32_t usb_read(void * buffer, uint32_t size)
{
    return tud_cdc_read(buffer, size);
}

/**
 * @brief Formatted printf-like function for USB CDC output.
 * 
 * @param format Printf formatting string
 * @param ... Arguments
 */
void usb_printf(const char * format, ...)
{
    if (!tud_cdc_connected())
    {
        return;
    }

    char _buf[164];
    va_list _args;
    va_start(_args, format);
    int _len = vsnprintf(_buf, sizeof(_buf), format, _args);
    va_end(_args);

    if (_len > 0)
    {
        usb_write(_buf, (uint32_t)_len);
    }
}

/**
 * @brief Check if a CDC terminal is connected.
 * 
 * @return true if connected, false otherwise
 */
bool usb_connected(void)
{
    return tud_cdc_connected();
}

/**
 * @brief USB Device interrupt handler.
 */
void USB_DRD_FS_IRQHandler(void)
{
    dcd_int_handler(0);
}
