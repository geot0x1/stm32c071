#ifndef BSP_USB_H
#define BSP_USB_H

#include "stm32c0xx_hal.h"

/**
 * @brief USB BSP handle.
 *
 * hal_handle points to the global hpcd_USB_DRD_FS defined in bsp_usb.c.
 * TinyUSB's STM32 port accesses the USB peripheral registers directly
 * and does not extern this symbol — the global exists for HAL MSP callbacks.
 */
typedef struct Usb_s
{
    PCD_HandleTypeDef *hal_handle;
} Usb_t;

/**
 * @brief Initialize the USB PCD peripheral.
 *
 * Fills hpcd_USB_DRD_FS, calls HAL_PCD_Init (which triggers
 * HAL_PCD_MspInit for USB clock source and clock enable).
 * board_init() should call HAL_Delay(20) after this for USB stabilization.
 *
 * @param usb  Handle to fill
 */
void usb_pcd_init(Usb_t *usb);

#endif /* BSP_USB_H */
