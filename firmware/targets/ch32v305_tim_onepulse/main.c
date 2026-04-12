/* ch32v305_tim_onepulse — TIM2 one-pulse mode self-test
 * Board: CH32V305RBT6
 * No new wiring required (TIM2_CH1 = PA0, already in Stage 2 setup).
 *
 * TIM2 in OPM (one-pulse mode), PSC=9599 (10kHz tick @ 96MHz), ARR=999.
 * PA0 (TIM2_CH1) = AF push-pull output (no external receiver needed).
 * TIM_Cmd starts counter directly (no slave trigger).
 * Counter runs one ARR period then stops; UIF flag is set.
 *
 * detail0 on PASS: TIM2->CNT (should be 0 after stop).
 */
#define AEL_MAILBOX_ADDR 0x20000600u
#include "ael_mailbox.h"
#include "ch32v30x.h"

int main(void)
{
    GPIO_InitTypeDef        GPIO_InitStructure        = {0};
    TIM_TimeBaseInitTypeDef TIM_TimeBaseInitStructure = {0};
    TIM_OCInitTypeDef       TIM_OCInitStructure       = {0};

    ael_mailbox_init();

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2, ENABLE);

    /* PA0: TIM2_CH1 AF push-pull output */
    GPIO_InitStructure.GPIO_Pin   = GPIO_Pin_0;
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_AF_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    /* TIM2: PSC=9599 → 10kHz tick (96MHz/9600); ARR=999 → 100ms period */
    TIM_TimeBaseInitStructure.TIM_Period        = 999;
    TIM_TimeBaseInitStructure.TIM_Prescaler     = 9600 - 1;
    TIM_TimeBaseInitStructure.TIM_ClockDivision = TIM_CKD_DIV1;
    TIM_TimeBaseInitStructure.TIM_CounterMode   = TIM_CounterMode_Up;
    TIM_TimeBaseInit(TIM2, &TIM_TimeBaseInitStructure);
    TIM_GenerateEvent(TIM2, TIM_EventSource_Update);  /* load PSC immediately */

    /* CH1: PWM2 output */
    TIM_OCInitStructure.TIM_OCMode      = TIM_OCMode_PWM2;
    TIM_OCInitStructure.TIM_OutputState = TIM_OutputState_Enable;
    TIM_OCInitStructure.TIM_Pulse       = 500;
    TIM_OCInitStructure.TIM_OCPolarity  = TIM_OCPolarity_High;
    TIM_OC1Init(TIM2, &TIM_OCInitStructure);

    /* One-pulse mode: counter stops after one update event */
    TIM_SelectOnePulseMode(TIM2, TIM_OPMode_Single);
    TIM_ClearFlag(TIM2, TIM_FLAG_Update);

    /* Start counter (no slave trigger — fires immediately) */
    TIM_Cmd(TIM2, ENABLE);

    /* Wait for UIF (one period elapsed, counter stopped) */
    uint32_t t = 2000000;
    while (!TIM_GetFlagStatus(TIM2, TIM_FLAG_Update) && --t);
    if (!t) {
        ael_mailbox_fail(1, TIM2->CNT);
        while (1);
    }

    uint32_t cnt = TIM2->CNT;
    ael_mailbox_pass();
    AEL_MAILBOX->detail0 = cnt;

    uint32_t tick = 0;
    while (1) {
        AEL_MAILBOX->detail0 = ++tick;
        for (volatile int i = 0; i < 960000; i++);
    }
}
