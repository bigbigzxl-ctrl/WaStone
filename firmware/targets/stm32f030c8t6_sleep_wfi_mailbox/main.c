#include <stdint.h>

#define AEL_MAILBOX_ADDR 0x20001C00u
#include "../stm32f030c8t6/ael_mailbox.h"

#define SYST_CSR (*(volatile uint32_t *)0xE000E010u)
#define SYST_RVR (*(volatile uint32_t *)0xE000E014u)
#define SYST_CVR (*(volatile uint32_t *)0xE000E018u)
#define SCB_SCR  (*(volatile uint32_t *)0xE000ED10u)

#define SYST_CSR_ENABLE    (1u << 0)
#define SYST_CSR_TICKINT   (1u << 1)
#define SYST_CSR_CLKSOURCE (1u << 2)
#define SCB_SCR_SLEEPDEEP  (1u << 2)

static volatile uint32_t g_ticks = 0u;
static volatile uint32_t g_wakeups = 0u;
static volatile uint32_t g_passed = 0u;

void SysTick_Handler(void)
{
    g_ticks++;
}

int main(void)
{
    ael_mailbox_init();

    SCB_SCR &= ~SCB_SCR_SLEEPDEEP;
    SYST_RVR = 7999u;
    SYST_CVR = 0u;
    SYST_CSR = SYST_CSR_CLKSOURCE | SYST_CSR_TICKINT | SYST_CSR_ENABLE;

    while (1) {
        __asm__ volatile ("wfi");
        g_wakeups++;
        AEL_MAILBOX->detail0 = g_wakeups;

        if (g_ticks != g_wakeups) {
            ael_mailbox_fail(0xE221u, (g_ticks << 16u) | (g_wakeups & 0xFFFFu));
            while (1) {}
        }
        if (g_wakeups >= 50u && g_passed == 0u) {
            ael_mailbox_pass();
            g_passed = 1u;
        }
    }
}
