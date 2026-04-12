/* ch32v305_adc_dma — ADC1 continuous + DMA1_Ch1 buffer fill
 * Board: CH32V305RBT6
 * No wiring required (Stage 1).
 *
 * ADC1 in continuous scan mode, DMA1_Ch1 reads ADC1->RDATAR → buf[16].
 * Channel: Vrefint (ch17) — no external pin needed.
 * Clock: 96MHz APB2 / 8 = 12 MHz ADC clock (within 14 MHz max).
 * Expected: ~1.2V at 3.3V supply → 12-bit raw ≈ 1489; accept 800..2100.
 * detail0 on PASS: first sample in buf.
 */
#define AEL_MAILBOX_ADDR 0x20000600u
#include "ael_mailbox.h"
#include "ch32v30x.h"

#define BUF_LEN 16
static volatile uint16_t adc_buf[BUF_LEN];

int main(void)
{
    ADC_InitTypeDef ADC_InitStructure = {0};
    DMA_InitTypeDef DMA_InitStructure = {0};

    ael_mailbox_init();

    /* Clocks */
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_ADC1, ENABLE);
    RCC_AHBPeriphClockCmd(RCC_AHBPeriph_DMA1, ENABLE);
    RCC_ADCCLKConfig(RCC_PCLK2_Div8);   /* 96MHz / 8 = 12 MHz */

    /* DMA1 Ch1: ADC1->RDATAR → adc_buf, HalfWord, Normal mode */
    DMA_DeInit(DMA1_Channel1);
    DMA_InitStructure.DMA_PeripheralBaseAddr = (uint32_t)&ADC1->RDATAR;
    DMA_InitStructure.DMA_MemoryBaseAddr     = (uint32_t)adc_buf;
    DMA_InitStructure.DMA_DIR                = DMA_DIR_PeripheralSRC;
    DMA_InitStructure.DMA_BufferSize         = BUF_LEN;
    DMA_InitStructure.DMA_PeripheralInc      = DMA_PeripheralInc_Disable;
    DMA_InitStructure.DMA_MemoryInc          = DMA_MemoryInc_Enable;
    DMA_InitStructure.DMA_PeripheralDataSize = DMA_PeripheralDataSize_HalfWord;
    DMA_InitStructure.DMA_MemoryDataSize     = DMA_MemoryDataSize_HalfWord;
    DMA_InitStructure.DMA_Mode               = DMA_Mode_Normal;
    DMA_InitStructure.DMA_Priority           = DMA_Priority_High;
    DMA_InitStructure.DMA_M2M                = DMA_M2M_Disable;
    DMA_Init(DMA1_Channel1, &DMA_InitStructure);
    DMA_Cmd(DMA1_Channel1, ENABLE);

    /* ADC1: continuous, scan, Vrefint */
    ADC_DeInit(ADC1);
    ADC_InitStructure.ADC_Mode               = ADC_Mode_Independent;
    ADC_InitStructure.ADC_ScanConvMode       = ENABLE;
    ADC_InitStructure.ADC_ContinuousConvMode = ENABLE;
    ADC_InitStructure.ADC_ExternalTrigConv   = ADC_ExternalTrigConv_None;
    ADC_InitStructure.ADC_DataAlign          = ADC_DataAlign_Right;
    ADC_InitStructure.ADC_NbrOfChannel       = 1;
    ADC_Init(ADC1, &ADC_InitStructure);

    ADC_TempSensorVrefintCmd(ENABLE);
    ADC_RegularChannelConfig(ADC1, ADC_Channel_Vrefint, 1, ADC_SampleTime_239Cycles5);

    ADC_Cmd(ADC1, ENABLE);

    /* Calibrate */
    ADC_BufferCmd(ADC1, DISABLE);
    ADC_ResetCalibration(ADC1);
    while (ADC_GetResetCalibrationStatus(ADC1));
    ADC_StartCalibration(ADC1);
    while (ADC_GetCalibrationStatus(ADC1));
    ADC_BufferCmd(ADC1, ENABLE);

    /* Enable ADC DMA request, start conversions */
    ADC_DMACmd(ADC1, ENABLE);
    ADC_SoftwareStartConvCmd(ADC1, ENABLE);

    /* Wait for DMA transfer complete (BUF_LEN samples) */
    uint32_t t = 2000000;
    while (DMA_GetFlagStatus(DMA1_FLAG_TC1) == RESET && --t);
    if (!t) { ael_mailbox_fail(1, 0); while (1); }

    ADC_SoftwareStartConvCmd(ADC1, DISABLE);
    DMA_Cmd(DMA1_Channel1, DISABLE);

    /* Verify all samples in range */
    uint32_t bad = 0;
    for (int i = 0; i < BUF_LEN; i++) {
        if (adc_buf[i] < 800 || adc_buf[i] > 2100) bad++;
    }

    if (bad) { ael_mailbox_fail(2, adc_buf[0]); }
    else     { ael_mailbox_pass(); AEL_MAILBOX->detail0 = adc_buf[0]; }

    uint32_t tick = 0;
    while (1) {
        AEL_MAILBOX->detail0 = ++tick;
        for (volatile int i = 0; i < 960000; i++);
    }
}
