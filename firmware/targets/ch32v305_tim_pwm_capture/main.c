/* ch32v305_tim_pwm_capture — TIM1 CH1 (PA8) PWM 1kHz → TIM2 CH1 (PA0) input capture
 * Board: CH32V305RBT6
 * Wiring: PA8 → PA0
 * Clock: 96 MHz
 *
 * TIM1 PSC=95 → 1MHz tick, ARR=999 → 1kHz PWM, 50% duty
 * TIM2 PSC=95 → 1MHz tick, captures rising edge on PA0
 * Expected: period = 1000 ticks ±5% = [950, 1050]
 * TIM2 is 32-bit on CH32V305, no masking needed.
 */
#define AEL_MAILBOX_ADDR 0x20000600u
#include "ael_mailbox.h"
#include "ch32v30x.h"

static void TIM1_PWM_Init(void)
{
    GPIO_InitTypeDef        GPIO_InitStructure        = {0};
    TIM_TimeBaseInitTypeDef TIM_TimeBaseInitStructure = {0};
    TIM_OCInitTypeDef       TIM_OCInitStructure       = {0};

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_TIM1, ENABLE);

    /* PA8: TIM1_CH1 AF push-pull */
    GPIO_InitStructure.GPIO_Pin   = GPIO_Pin_8;
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_AF_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    TIM_TimeBaseInitStructure.TIM_Period        = 1000 - 1;  /* ARR: 1kHz at 1MHz tick */
    TIM_TimeBaseInitStructure.TIM_Prescaler     = 96 - 1;    /* PSC: 96MHz/96 = 1MHz  */
    TIM_TimeBaseInitStructure.TIM_ClockDivision = TIM_CKD_DIV1;
    TIM_TimeBaseInitStructure.TIM_CounterMode   = TIM_CounterMode_Up;
    TIM_TimeBaseInitStructure.TIM_RepetitionCounter = 0;
    TIM_TimeBaseInit(TIM1, &TIM_TimeBaseInitStructure);
    TIM_GenerateEvent(TIM1, TIM_EventSource_Update);  /* load PSC immediately */

    TIM_OCInitStructure.TIM_OCMode      = TIM_OCMode_PWM1;
    TIM_OCInitStructure.TIM_OutputState = TIM_OutputState_Enable;
    TIM_OCInitStructure.TIM_Pulse       = 500 - 1;           /* 50% duty */
    TIM_OCInitStructure.TIM_OCPolarity  = TIM_OCPolarity_High;
    TIM_OC1Init(TIM1, &TIM_OCInitStructure);

    TIM_CtrlPWMOutputs(TIM1, ENABLE);
    TIM_OC1PreloadConfig(TIM1, TIM_OCPreload_Enable);
    TIM_ARRPreloadConfig(TIM1, ENABLE);
    TIM_Cmd(TIM1, ENABLE);
}

static void TIM2_Capture_Init(void)
{
    GPIO_InitTypeDef        GPIO_InitStructure        = {0};
    TIM_TimeBaseInitTypeDef TIM_TimeBaseInitStructure = {0};
    TIM_ICInitTypeDef       TIM_ICInitStructure       = {0};

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2, ENABLE);

    /* PA0: TIM2_CH1 input floating */
    GPIO_InitStructure.GPIO_Pin  = GPIO_Pin_0;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    TIM_TimeBaseInitStructure.TIM_Period        = 0xFFFFFFFF;  /* 32-bit TIM2 */
    TIM_TimeBaseInitStructure.TIM_Prescaler     = 96 - 1;      /* 1MHz tick */
    TIM_TimeBaseInitStructure.TIM_ClockDivision = TIM_CKD_DIV1;
    TIM_TimeBaseInitStructure.TIM_CounterMode   = TIM_CounterMode_Up;
    TIM_TimeBaseInit(TIM2, &TIM_TimeBaseInitStructure);
    TIM_GenerateEvent(TIM2, TIM_EventSource_Update);

    TIM_ICInitStructure.TIM_Channel     = TIM_Channel_1;
    TIM_ICInitStructure.TIM_ICPolarity  = TIM_ICPolarity_Rising;
    TIM_ICInitStructure.TIM_ICSelection = TIM_ICSelection_DirectTI;
    TIM_ICInitStructure.TIM_ICPrescaler = TIM_ICPSC_DIV1;
    TIM_ICInitStructure.TIM_ICFilter    = 0x00;
    TIM_ICInit(TIM2, &TIM_ICInitStructure);

    TIM_Cmd(TIM2, ENABLE);
}

int main(void)
{
    ael_mailbox_init();

    TIM1_PWM_Init();
    TIM2_Capture_Init();

    /* Wait for first capture */
    uint32_t t = 500000;
    while (TIM_GetFlagStatus(TIM2, TIM_FLAG_CC1) == RESET && --t);
    if (!t) { ael_mailbox_fail(1, 0); while (1); }
    uint32_t cap0 = TIM_GetCapture1(TIM2);
    TIM_ClearFlag(TIM2, TIM_FLAG_CC1);

    /* Wait for second capture */
    t = 500000;
    while (TIM_GetFlagStatus(TIM2, TIM_FLAG_CC1) == RESET && --t);
    if (!t) { ael_mailbox_fail(2, 0); while (1); }
    uint32_t cap1 = TIM_GetCapture1(TIM2);

    /* Period = difference; TIM2 is 32-bit, no masking needed */
    uint32_t period = cap1 - cap0;

    /* At 1MHz, 1kHz PWM → period = 1000 ticks; allow ±5% = [950, 1050] */
    if (period < 950 || period > 1050) {
        ael_mailbox_fail(3, period);
    } else {
        ael_mailbox_pass();
    }

    uint32_t tick = 0;
    while (1) {
        AEL_MAILBOX->detail0 = ++tick;
        for (volatile int i = 0; i < 960000; i++);
    }
}
