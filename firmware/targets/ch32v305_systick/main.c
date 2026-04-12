/* ch32v305_systick — SysTick liveness: detail0 increments every ~1s */
#define AEL_MAILBOX_ADDR 0x20000600u
#include "ael_mailbox.h"
#include "ch32v30x.h"

int main(void)
{
    ael_mailbox_init();
    SysTick->CTLR = 0;
    SysTick->SR   = 0;
    SysTick->CNT  = 0;
    SysTick->CMP  = SystemCoreClock - 1; /* 1 Hz tick at 96 MHz */
    SysTick->CTLR = 0xF;
    ael_mailbox_pass();
    uint32_t tick = 0;
    while (1) {
        if (SysTick->SR & 1) {
            SysTick->SR = 0;
            AEL_MAILBOX->detail0 = ++tick;
        }
    }
}
