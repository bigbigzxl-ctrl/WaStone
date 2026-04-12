/* ch32v203_adc_temp — HAL ADC1: read internal temperature sensor (channel 16)
 * Based on EVT/EXAM/ADC/Internal_Temperature pattern
 * Temp formula: T = (V25 - Vsense) / Avg_Slope + 25
 *   V25 ~= 1.43V, Avg_Slope = 4.3 mV/°C
 *   At 25°C, 3.3V Vref, 12-bit: ~1775; accept 1000..2800 (−40°C..125°C range)
 * detail0 on PASS: raw ADC reading
 */
#define AEL_MAILBOX_ADDR 0x20000600u
#include "ael_mailbox.h"
#include "ch32v20x.h"

static void ADC1_Init(void)
{
    ADC_InitTypeDef ADC_InitStructure = {0};

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_ADC1, ENABLE);
    RCC_ADCCLKConfig(RCC_PCLK2_Div8);  /* ADC clock = 72MHz/8 = 9MHz */

    ADC_DeInit(ADC1);
    ADC_InitStructure.ADC_Mode               = ADC_Mode_Independent;
    ADC_InitStructure.ADC_ScanConvMode       = DISABLE;
    ADC_InitStructure.ADC_ContinuousConvMode = DISABLE;
    ADC_InitStructure.ADC_ExternalTrigConv   = ADC_ExternalTrigConv_None;
    ADC_InitStructure.ADC_DataAlign          = ADC_DataAlign_Right;
    ADC_InitStructure.ADC_NbrOfChannel       = 1;
    ADC_Init(ADC1, &ADC_InitStructure);

    ADC_Cmd(ADC1, ENABLE);

    /* Calibrate */
    ADC_BufferCmd(ADC1, DISABLE);
    ADC_ResetCalibration(ADC1);
    while (ADC_GetResetCalibrationStatus(ADC1));
    ADC_StartCalibration(ADC1);
    while (ADC_GetCalibrationStatus(ADC1));
    ADC_BufferCmd(ADC1, ENABLE);

    /* Enable internal temperature sensor (and Vrefint) */
    ADC_TempSensorVrefintCmd(ENABLE);
}

int main(void)
{
    ael_mailbox_init();
    ADC1_Init();

    /* Configure channel 16 (TempSensor), rank 1, max sample time */
    ADC_RegularChannelConfig(ADC1, ADC_Channel_TempSensor, 1, ADC_SampleTime_239Cycles5);

    /* Start conversion */
    ADC_SoftwareStartConvCmd(ADC1, ENABLE);
    uint32_t t = 500000;
    while (!ADC_GetFlagStatus(ADC1, ADC_FLAG_EOC) && --t);
    if (!t) { ael_mailbox_fail(1, 0); while (1); }

    uint32_t raw = ADC_GetConversionValue(ADC1);

    /* Sanity check: at 3.3V ref, 12-bit, raw ~1775 at 25°C
     * Accept 1000..2800 (covers −40°C to 125°C) */
    if (raw < 1000 || raw > 2800) {
        ael_mailbox_fail(2, raw);
    } else {
        ael_mailbox_pass();
        AEL_MAILBOX->detail0 = raw;
    }

    uint32_t tick = 0;
    while (1) {
        AEL_MAILBOX->detail0 = raw;
        for (volatile int i = 0; i < 720000; i++);
        tick++;
    }
}
