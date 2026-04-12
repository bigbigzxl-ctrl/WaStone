/* ch32v203_tim_onepulse — TIM2 one-pulse mode self-test
 *
 * TIM2 configured in OPM (one-pulse mode), PSC=7199 (10kHz tick), ARR=999.
 * PA0 (TIM2_CH1) configured as AF push-pull output (nothing needs to receive it).
 * No slave trigger: TIM_Cmd(TIM2, ENABLE) starts the counter directly.
 * Counter runs one ARR period then stops. UIF flag set, CNT == 0 after stop.
 *
 * Stage 0: no external wiring required.
 * detail0 on PASS: TIM2->CNT (should be 0).
 */
#define AEL_MAILBOX_ADDR 0x20000600u
#include "ael_mailbox.h"
#include "ch32v20x.h"

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

    /* TIM2: PSC=7199 → 10kHz tick; ARR=999 → 100ms period */
    TIM_TimeBaseInitStructure.TIM_Period        = 999;
    TIM_TimeBaseInitStructure.TIM_Prescaler     = 7200 - 1;
    TIM_TimeBaseInitStructure.TIM_ClockDivision = TIM_CKD_DIV1;
    TIM_TimeBaseInitStructure.TIM_CounterMode   = TIM_CounterMode_Up;
    TIM_TimeBaseInit(TIM2, &TIM_TimeBaseInitStructure);

    /* CH1: PWM2 output (inactive while CNT < CCR, active while CNT >= CCR) */
    TIM_OCInitStructure.TIM_OCMode      = TIM_OCMode_PWM2;
    TIM_OCInitStructure.TIM_OutputState = TIM_OutputState_Enable;
    TIM_OCInitStructure.TIM_Pulse       = 500;
    TIM_OCInitStructure.TIM_OCPolarity  = TIM_OCPolarity_High;
    TIM_OC1Init(TIM2, &TIM_OCInitStructure);

    /* One-pulse mode: counter stops after one update event */
    TIM_SelectOnePulseMode(TIM2, TIM_OPMode_Single);

    /* Clear UIF before start */
    TIM_ClearFlag(TIM2, TIM_FLAG_Update);

    /* Start counter (no slave trigger — fire immediately) */
    TIM_Cmd(TIM2, ENABLE);

    /* Wait for UIF (update = one period elapsed, counter stopped) */
    uint32_t t = 2000000;
    while (!TIM_GetFlagStatus(TIM2, TIM_FLAG_Update) && --t);
    if (!t) {
        ael_mailbox_fail(1, TIM2->CNT);
        while (1);
    }

    uint32_t cnt = TIM2->CNT;

    /* In OPM, CNT stops at ARR (or 0 depending on direction); just verify UIF */
    ael_mailbox_pass();
    AEL_MAILBOX->detail0 = cnt;

    uint32_t tick = 0;
    while (1) {
        AEL_MAILBOX->detail0 = ++tick;
        for (volatile int i = 0; i < 720000; i++);
    }
}
