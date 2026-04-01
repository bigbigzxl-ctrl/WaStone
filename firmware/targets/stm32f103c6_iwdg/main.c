#include <stdint.h>

#define AEL_MAILBOX_ADDR 0x20002400u
#include "../stm32f103c6/ael_mailbox.h"

#define RCC_BASE   0x40021000u
#define RCC_CSR    (*(volatile uint32_t *)(RCC_BASE + 0x24u))
#define RCC_CSR_LSION  (1u << 0)
#define RCC_CSR_LSIRDY (1u << 1)

#define IWDG_BASE  0x40003000u
#define IWDG_KR    (*(volatile uint32_t *)(IWDG_BASE + 0x00u))
#define IWDG_PR    (*(volatile uint32_t *)(IWDG_BASE + 0x04u))
#define IWDG_RLR   (*(volatile uint32_t *)(IWDG_BASE + 0x08u))

#define IWDG_KEY_RELOAD  0xAAAAu
#define IWDG_KEY_UNLOCK  0x5555u
#define IWDG_KEY_START   0xCCCCu

#define SYST_CSR (*(volatile uint32_t *)0xE000E010u)
#define SYST_RVR (*(volatile uint32_t *)0xE000E014u)
#define SYST_CVR (*(volatile uint32_t *)0xE000E018u)
#define SYST_CSR_ENABLE    (1u << 0)
#define SYST_CSR_CLKSOURCE (1u << 2)
#define SYST_CSR_COUNTFLAG (1u << 16)

static void delay_ms(uint32_t ms)
{
    for (uint32_t i = 0u; i < ms; i++) {
        SYST_CVR = 0u;
        while ((SYST_CSR & SYST_CSR_COUNTFLAG) == 0u) {}
    }
}

int main(void)
{
    SYST_RVR = 7999u;
    SYST_CVR = 0u;
    SYST_CSR = SYST_CSR_CLKSOURCE | SYST_CSR_ENABLE;

    ael_mailbox_init();

    RCC_CSR |= RCC_CSR_LSION;
    while ((RCC_CSR & RCC_CSR_LSIRDY) == 0u) {}

    IWDG_KR  = IWDG_KEY_UNLOCK;
    IWDG_PR  = 0x06u;
    IWDG_RLR = 4095u;
    IWDG_KR  = IWDG_KEY_RELOAD;
    IWDG_KR  = IWDG_KEY_START;

    ael_mailbox_pass();

    uint32_t iteration = 0u;
    while (1) {
        delay_ms(1u);
        IWDG_KR = IWDG_KEY_RELOAD;
        AEL_MAILBOX->detail0 = ++iteration;
    }
}
