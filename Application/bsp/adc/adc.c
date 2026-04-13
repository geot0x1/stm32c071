#include "adc.h"
#include "main.h"
#include "stm32c0xx_ll_adc.h"

static ADC_HandleTypeDef hadc1;

void adc_init(void)
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

ADC_HandleTypeDef *adc_get_handle(void)
{
    return &hadc1;
}

bool adc_read_channel(uint32_t channel, uint32_t *out_raw)
{
    if (out_raw == NULL)
    {
        return false;
    }

    ADC_ChannelConfTypeDef s_config = {0};
    s_config.Channel = channel;
    s_config.Rank    = ADC_RANK_CHANNEL_NUMBER;

    if (HAL_ADC_ConfigChannel(&hadc1, &s_config) != HAL_OK)
    {
        return false;
    }

    /* Workaround for SCAN_SEQ_FIXED mode cross-contamination:
     * In SCAN_SEQ_FIXED mode, both TEMPSENSOR (ch9) and VREFINT (ch10) remain active
     * in CHSELR after adc_init(). Since ch9 < ch10, it always converts first, and
     * EOC fires after ch9, causing HAL_ADC_GetValue() to return ch9's result regardless
     * of which channel was requested.
     *
     * Workaround: Clear CHSELR and reconfigure only the requested channel using
     * LL functions to directly manipulate the register. This ensures only one channel
     * is active during the conversion.
     */
    LL_ADC_REG_SetSequencerChannels(hadc1.Instance, channel);

    if (HAL_ADC_Start(&hadc1) != HAL_OK)
    {
        return false;
    }

    /* Poll for conversion with 10 ms timeout (~7 µs actual conversion time) */
    if (HAL_ADC_PollForConversion(&hadc1, 10) != HAL_OK)
    {
        HAL_ADC_Stop(&hadc1);
        /* Restore both channels before returning error */
        LL_ADC_REG_SetSequencerChannels(hadc1.Instance, ADC_CHANNEL_TEMPSENSOR | ADC_CHANNEL_VREFINT);
        return false;
    }

    *out_raw = HAL_ADC_GetValue(&hadc1);
    HAL_ADC_Stop(&hadc1);

    /* Restore both channels to CHSELR for future reads */
    LL_ADC_REG_SetSequencerChannels(hadc1.Instance, ADC_CHANNEL_TEMPSENSOR | ADC_CHANNEL_VREFINT);

    return true;
}
