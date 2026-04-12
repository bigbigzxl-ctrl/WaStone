/* ch32v203_tim_encoder — TIM2 quadrature generator → TIM3 encoder decoder
 * Based on EVT/EXAM/TIM/Encoder
 *
 * TIM2: toggle mode on PA0 (CH1) and PA1 (CH2), PSC=71 (1MHz), ARR=999 (1kHz toggle)
 *       Generates quadrature-like signals with 90° phase offset (CCR2 = CCR1 - 200)
 * TIM3: encoder mode TI12 on PA6 (CH1) + PA7 (CH2), ARR=1000
 *       Counts edges from both channels; CNT increments/decrements
 *
 * Wiring (Stage 2 new connections):
 *   PA0 → PA6   (TIM2_CH1 → TIM3_CH1)
 *   PA1 → PA7   (TIM2_CH2 → TIM3_CH2)
 *
 * After 50ms, TIM3->CNT should differ from 0 (pulses received).
 * detail0 on PASS: final TIM3 CNT value
 */
#define AEL_MAILBOX_ADDR 0x20000600u
#include "ael_mailbox.h"
#include "ch32v20x.h"

static void delay_ms_approx(uint32_t ms)
{
    /* at 72MHz, 1 nop ≈ 14ns; 72000 nops ≈ 1ms */
    for (uint32_t i = 0; i < ms * 72000; i++) __asm__("nop");
}

static void TIM2_Generator_Init(void)
{
    GPIO_InitTypeDef        GPIO_InitStructure        = {0};
    TIM_TimeBaseInitTypeDef TIM_TimeBaseInitStructure = {0};
    TIM_OCInitTypeDef       TIM_OCInitStructure       = {0};

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2, ENABLE);

    /* PA0: TIM2_CH1, PA1: TIM2_CH2 — AF push-pull */
    GPIO_InitStructure.GPIO_Pin   = GPIO_Pin_0 | GPIO_Pin_1;
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_AF_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    TIM_TimeBaseInitStructure.TIM_Period        = 1000 - 1;  /* ARR: 1kHz toggle */
    TIM_TimeBaseInitStructure.TIM_Prescaler     = 72 - 1;    /* PSC: 72MHz/72=1MHz */
    TIM_TimeBaseInitStructure.TIM_ClockDivision = TIM_CKD_DIV1;
    TIM_TimeBaseInitStructure.TIM_CounterMode   = TIM_CounterMode_Up;
    TIM_TimeBaseInit(TIM2, &TIM_TimeBaseInitStructure);

    TIM_OCInitStructure.TIM_OCMode      = TIM_OCMode_Toggle;
    TIM_OCInitStructure.TIM_OutputState = TIM_OutputState_Enable;
    TIM_OCInitStructure.TIM_OCPolarity  = TIM_OCPolarity_High;

    TIM_OCInitStructure.TIM_Pulse = 500;      /* CH1 toggle at 500 */
    TIM_OC1Init(TIM2, &TIM_OCInitStructure);
    TIM_OC1PreloadConfig(TIM2, TIM_OCPreload_Disable);

    TIM_OCInitStructure.TIM_Pulse = 300;      /* CH2 toggle at 300 (phase offset) */
    TIM_OC2Init(TIM2, &TIM_OCInitStructure);
    TIM_OC2PreloadConfig(TIM2, TIM_OCPreload_Disable);

    TIM_ARRPreloadConfig(TIM2, ENABLE);
    TIM_Cmd(TIM2, ENABLE);
}

static void TIM3_Encoder_Init(void)
{
    GPIO_InitTypeDef        GPIO_InitStructure        = {0};
    TIM_TimeBaseInitTypeDef TIM_TimeBaseInitStructure = {0};
    TIM_ICInitTypeDef       TIM_ICInitStructure       = {0};

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM3, ENABLE);

    /* PA6: TIM3_CH1, PA7: TIM3_CH2 — input floating */
    GPIO_InitStructure.GPIO_Pin  = GPIO_Pin_6 | GPIO_Pin_7;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    TIM_TimeBaseInitStructure.TIM_Period      = 1000 - 1;
    TIM_TimeBaseInitStructure.TIM_Prescaler   = 0;
    TIM_TimeBaseInitStructure.TIM_ClockDivision = TIM_CKD_DIV1;
    TIM_TimeBaseInitStructure.TIM_CounterMode = TIM_CounterMode_Up;
    TIM_TimeBaseInit(TIM3, &TIM_TimeBaseInitStructure);

    TIM_EncoderInterfaceConfig(TIM3, TIM_EncoderMode_TI12,
                               TIM_ICPolarity_Rising, TIM_ICPolarity_Rising);

    TIM_ICStructInit(&TIM_ICInitStructure);
    TIM_ICInitStructure.TIM_ICFilter = 10;
    TIM_ICInit(TIM3, &TIM_ICInitStructure);

    TIM_SetCounter(TIM3, 0);
    TIM_Cmd(TIM3, ENABLE);
}

int main(void)
{
    ael_mailbox_init();

    TIM3_Encoder_Init();   /* init decoder first */
    TIM2_Generator_Init(); /* then start generator */

    /* Wait ~50ms for TIM2 to generate pulses */
    delay_ms_approx(50);

    uint32_t cnt = TIM3->CNT;

    /* If wired correctly, CNT should have moved from 0 */
    if (cnt == 0) {
        ael_mailbox_fail(1, 0);
    } else {
        ael_mailbox_pass();
        AEL_MAILBOX->detail0 = cnt;
    }

    uint32_t tick = 0;
    while (1) {
        AEL_MAILBOX->detail0 = TIM3->CNT;
        for (volatile int i = 0; i < 720000; i++);
        tick++;
    }
}
