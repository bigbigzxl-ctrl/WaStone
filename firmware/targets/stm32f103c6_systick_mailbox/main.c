#include <stdint.h>

#define AEL_MAILBOX_ADDR 0x20002400u
#include "../stm32f103c6/ael_mailbox.h"

#define SYST_CSR (*(volatile uint32_t *)0xE000E010u)
#define SYST_RVR (*(volatile uint32_t *)0xE000E014u)
#define SYST_CVR (*(volatile uint32_t *)0xE000E018u)

#define SYST_CSR_ENABLE    (1u << 0)
#define SYST_CSR_TICKINT   (1u << 1)
#define SYST_CSR_CLKSOURCE (1u << 2)

static volatile uint32_t g_ticks = 0u;
static volatile uint32_t g_passed = 0u;

void SysTick_Handler(void)
{
    g_ticks++;
    AEL_MAILBOX->detail0 = g_ticks;
    if (g_ticks >= 100u && g_passed == 0u) {
        ael_mailbox_pass();
        g_passed = 1u;
    }
}

int main(void)
{
    ael_mailbox_init();

    SYST_RVR = 7999u;
    SYST_CVR = 0u;
    SYST_CSR = SYST_CSR_CLKSOURCE | SYST_CSR_TICKINT | SYST_CSR_ENABLE;

    while (1) {}
}
