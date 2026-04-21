#include "bsp_usb.h"

/* Global PCD handle — name is preserved from STM32CubeMX generation.
 * HAL_PCD_MspInit (in stm32c0xx_hal_msp.c) receives this via pointer
 * and configures the USB clock source + clock enable. */
PCD_HandleTypeDef hpcd_USB_DRD_FS;

void usb_pcd_init(Usb *usb)
{
    hpcd_USB_DRD_FS.Instance                      = USB_DRD_FS;
    hpcd_USB_DRD_FS.Init.dev_endpoints            = 8;
    hpcd_USB_DRD_FS.Init.speed                    = USBD_FS_SPEED;
    hpcd_USB_DRD_FS.Init.phy_itface               = PCD_PHY_EMBEDDED;
    hpcd_USB_DRD_FS.Init.Sof_enable               = DISABLE;
    hpcd_USB_DRD_FS.Init.low_power_enable         = DISABLE;
    hpcd_USB_DRD_FS.Init.lpm_enable               = DISABLE;
    hpcd_USB_DRD_FS.Init.battery_charging_enable  = DISABLE;
    hpcd_USB_DRD_FS.Init.vbus_sensing_enable      = DISABLE;
    hpcd_USB_DRD_FS.Init.bulk_doublebuffer_enable = DISABLE;
    hpcd_USB_DRD_FS.Init.iso_singlebuffer_enable  = DISABLE;

    /* HAL_PCD_MspInit fires here and configures the USB clock. */
    HAL_PCD_Init(&hpcd_USB_DRD_FS);

    usb->hal_handle = &hpcd_USB_DRD_FS;
}
