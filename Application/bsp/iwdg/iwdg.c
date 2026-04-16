#include "iwdg.h"
#include <string.h>

void iwdg_init(Iwdg_t *iwdg, uint32_t prescaler, uint32_t reload, uint32_t window)
{
    memset(iwdg, 0, sizeof(Iwdg_t));

    iwdg->hal_handle.Instance       = IWDG;
    iwdg->hal_handle.Init.Prescaler = prescaler;
    iwdg->hal_handle.Init.Reload    = reload;
    iwdg->hal_handle.Init.Window    = window;

    HAL_IWDG_Init(&iwdg->hal_handle);
}

Iwdg_err_t iwdg_refresh(Iwdg_t *iwdg)
{
    if (HAL_IWDG_Refresh(&iwdg->hal_handle) != HAL_OK)
    {
        return IWDG_ERR_HAL;
    }

    return IWDG_OK;
}
