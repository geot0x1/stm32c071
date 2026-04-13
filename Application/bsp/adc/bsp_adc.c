#include "bsp_adc.h"
#include "main.h"

static ADC_HandleTypeDef hadc1;

void bsp_adc_init(void)
{
    ADC_ChannelConfTypeDef s_config = {0};

    hadc1.Instance                   = ADC1;
    hadc1.Init.ClockPrescaler        = ADC_CLOCK_SYNC_PCLK_DIV2;
    hadc1.Init.Resolution            = ADC_RESOLUTION_12B;
    hadc1.Init.DataAlign             = ADC_DATAALIGN_RIGHT;
    hadc1.Init.ScanConvMode          = ADC_SCAN_SEQ_FIXED;
    hadc1.Init.EOCSelection          = ADC_EOC_SINGLE_CONV;
    hadc1.Init.LowPowerAutoWait      = DISABLE;
    hadc1.Init.LowPowerAutoPowerOff  = DISABLE;
    hadc1.Init.ContinuousConvMode    = DISABLE;
    hadc1.Init.NbrOfConversion       = 1;
    hadc1.Init.DiscontinuousConvMode = DISABLE;
    hadc1.Init.ExternalTrigConv      = ADC_SOFTWARE_START;
    hadc1.Init.ExternalTrigConvEdge  = ADC_EXTERNALTRIGCONVEDGE_NONE;
    hadc1.Init.DMAContinuousRequests = DISABLE;
    hadc1.Init.Overrun               = ADC_OVR_DATA_PRESERVED;
    hadc1.Init.SamplingTimeCommon1   = ADC_SAMPLETIME_160CYCLES_5;
    hadc1.Init.OversamplingMode      = DISABLE;
    hadc1.Init.TriggerFrequencyMode  = ADC_TRIGGER_FREQ_HIGH;

    if (HAL_ADC_Init(&hadc1) != HAL_OK)
    {
        Error_Handler();
    }

    s_config.Channel = ADC_CHANNEL_TEMPSENSOR;
    s_config.Rank    = ADC_RANK_CHANNEL_NUMBER;
    if (HAL_ADC_ConfigChannel(&hadc1, &s_config) != HAL_OK)
    {
        Error_Handler();
    }

    s_config.Channel = ADC_CHANNEL_VREFINT;
    if (HAL_ADC_ConfigChannel(&hadc1, &s_config) != HAL_OK)
    {
        Error_Handler();
    }
}

ADC_HandleTypeDef *bsp_adc_get_handle(void)
{
    return &hadc1;
}
