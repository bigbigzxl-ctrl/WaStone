/*
 * stm32h563rgt6_tim151617_mailbox — TIM15/16/17 counter self-test
 *
 * Tests the three APB2 advanced-control/general-purpose timers:
 *   TIM15 — 16-bit, 2-channel, complementary outputs
 *   TIM16 — 16-bit, 1-channel, complementary output
 *   TIM17 — 16-bit, 1-channel, complementary output
 *
 * These differ from the main TIM1/TIM8 advanced timers in that they have
 * fewer channels but the same basic counter mechanism.
 *
 * Method: enable each timer, set PSC=0 (no prescaler), set ARR=0xFFFF,
 * enable counter (CEN=1), spin, read CNT twice, verify CNT advanced.
 *
 * TIM15_BASE = 0x40014000 (APB2 + 0x4000)
 * TIM16_BASE = 0x40014400 (APB2 + 0x4400)
 * TIM17_BASE = 0x40014800 (APB2 + 0x4800)
 *
 * RCC_APB2ENR = RCC_BASE + 0x0ACu (confirmed from existing spi/uart tests)
 *   bit16 = TIM15EN
 *   bit17 = TIM16EN
 *   bit18 = TIM17EN
 *
 * TIM register offsets (standard STM32 general-purpose timer):
 *   CR1  +0x00: bit0=CEN
 *   PSC  +0x28: prescaler
 *   ARR  +0x2C: auto-reload
 *   CNT  +0x24: counter
 *   EGR  +0x14: bit0=UG (update generation — loads PSC immediately)
 *
 * Note: PSC is double-buffered. Must write EGR.UG=1 after PSC write
 *       (see HIGH_PRIORITY pattern 27de4499).
 *
 * FAIL codes:
 *   0xE001 — TIM15 CNT did not advance
 *   0xE002 — TIM16 CNT did not advance
 *   0xE003 — TIM17 CNT did not advance
 *
 * detail0: [31:16]=TIM15_CNT, [15:8]=TIM16_CNT[7:0], [7:0]=TIM17_CNT[7:0]
 */

#include <stdint.h>
#include "../ael_mailbox.h"

#define RCC_BASE        0x44020C00u
#define RCC_APB2ENR     (*(volatile uint32_t *)(RCC_BASE + 0x0ACu))

#define TIM15_BASE      0x40014000u
#define TIM16_BASE      0x40014400u
#define TIM17_BASE      0x40014800u

/* Standard TIM register offsets */
#define TIM_CR1_OFF     0x00u
#define TIM_EGR_OFF     0x14u
#define TIM_CNT_OFF     0x24u
#define TIM_PSC_OFF     0x28u
#define TIM_ARR_OFF     0x2Cu

#define TIM_CR1_CEN     (1u << 0)
#define TIM_EGR_UG      (1u << 0)

static inline volatile uint32_t *tim_reg(uint32_t base, uint32_t off)
{
    return (volatile uint32_t *)(base + off);
}

static void tim_start(uint32_t base)
{
    *tim_reg(base, TIM_CR1_OFF) = 0u;        /* disable first */
    *tim_reg(base, TIM_PSC_OFF) = 0u;        /* no prescaler (max speed) */
    *tim_reg(base, TIM_ARR_OFF) = 0xFFFFu;   /* count up to 65535 */
    *tim_reg(base, TIM_EGR_OFF) = TIM_EGR_UG; /* load PSC immediately */
    *tim_reg(base, TIM_CR1_OFF) = TIM_CR1_CEN; /* start */
}

static uint32_t tim_cnt(uint32_t base)
{
    return *tim_reg(base, TIM_CNT_OFF);
}

int main(void)
{
    ael_mailbox_init();

    /* Enable TIM15/16/17 clocks (APB2ENR bits 16,17,18) */
    RCC_APB2ENR |= (1u << 16) | (1u << 17) | (1u << 18);
    (void)RCC_APB2ENR;

    /* Start all three timers */
    tim_start(TIM15_BASE);
    tim_start(TIM16_BASE);
    tim_start(TIM17_BASE);

    /* Spin delay */
    for (volatile uint32_t d = 0u; d < 10000u; d++) {}

    uint32_t cnt15a = tim_cnt(TIM15_BASE);
    uint32_t cnt16a = tim_cnt(TIM16_BASE);
    uint32_t cnt17a = tim_cnt(TIM17_BASE);

    for (volatile uint32_t d = 0u; d < 10000u; d++) {}

    uint32_t cnt15b = tim_cnt(TIM15_BASE);
    uint32_t cnt16b = tim_cnt(TIM16_BASE);
    uint32_t cnt17b = tim_cnt(TIM17_BASE);

    if (cnt15a == cnt15b) { ael_mailbox_fail(0xE001u, cnt15a); while (1) {} }
    if (cnt16a == cnt16b) { ael_mailbox_fail(0xE002u, cnt16a); while (1) {} }
    if (cnt17a == cnt17b) { ael_mailbox_fail(0xE003u, cnt17a); while (1) {} }

    /* detail0: [31:16]=TIM15_CNT, [15:8]=TIM16_CNT[7:0], [7:0]=TIM17_CNT[7:0] */
    AEL_MAILBOX->detail0 = ((cnt15b & 0xFFFFu) << 16)
                         | ((cnt16b & 0xFFu)   << 8)
                         | (cnt17b  & 0xFFu);
    ael_mailbox_pass();
    while (1) {}
    return 0;
}
