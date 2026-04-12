/* ch32v305_adc_vref — HAL ADC1: read internal Vrefint (channel 17)
 * Expected: ~1.2V at 3.3V Vref, 12-bit → ~1489; accept 800..1900
 * ADC clock = 96MHz/8 = 12MHz (max 14MHz for CH32V30x)
 */
#define AEL_MAILBOX_ADDR 0x20000600u
#include "ael_mailbox.h"
#include "ch32v30x.h"

static void ADC1_Init(void)
{
    ADC_InitTypeDef ADC_InitStructure = {0};

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_ADC1, ENABLE);
    RCC_ADCCLKConfig(RCC_PCLK2_Div8);  /* ADC clock = 96MHz/8 = 12MHz */

    ADC_DeInit(ADC1);
    ADC_InitStructure.ADC_Mode               = ADC_Mode_Independent;
    ADC_InitStructure.ADC_ScanConvMode       = DISABLE;
    ADC_InitStructure.ADC_ContinuousConvMode = DISABLE;
    ADC_InitStructure.ADC_ExternalTrigConv   = ADC_ExternalTrigConv_None;
    ADC_InitStructure.ADC_DataAlign          = ADC_DataAlign_Right;
    ADC_InitStructure.ADC_NbrOfChannel       = 1;
    ADC_Init(ADC1, &ADC_InitStructure);

    ADC_Cmd(ADC1, ENABLE);

    ADC_BufferCmd(ADC1, DISABLE);
    ADC_ResetCalibration(ADC1);
    while (ADC_GetResetCalibrationStatus(ADC1));
    ADC_StartCalibration(ADC1);
    while (ADC_GetCalibrationStatus(ADC1));
    ADC_BufferCmd(ADC1, ENABLE);

    ADC_TempSensorVrefintCmd(ENABLE);
}

int main(void)
{
    ael_mailbox_init();
    ADC1_Init();

    ADC_RegularChannelConfig(ADC1, ADC_Channel_Vrefint, 1, ADC_SampleTime_239Cycles5);

    ADC_SoftwareStartConvCmd(ADC1, ENABLE);
    uint32_t t = 500000;
    while (!ADC_GetFlagStatus(ADC1, ADC_FLAG_EOC) && --t);
    if (!t) { ael_mailbox_fail(1, 0); while (1); }

    uint32_t raw = ADC_GetConversionValue(ADC1);

    if (raw < 800 || raw > 1900) {
        ael_mailbox_fail(2, raw);
    } else {
        ael_mailbox_pass();
    }

    uint32_t tick = 0;
    while (1) {
        AEL_MAILBOX->detail0 = ++tick;
        for (volatile int i = 0; i < 960000; i++);
    }
}
