/* ch32v305_iwdg — IWDG start, pet, and verify running */
#define AEL_MAILBOX_ADDR 0x20000600u
#include "ael_mailbox.h"
#include "ch32v30x.h"

int main(void)
{
    ael_mailbox_init();
    IWDG->CTLR = 0xCCCC;
    IWDG->CTLR = 0x5555;
    IWDG->PSCR = 4;      /* /64 */
    IWDG->RLDR = 0xFFF;
    uint32_t timeout = 100000;
    while ((IWDG->STATR & 3) && --timeout);
    if (!timeout) { ael_mailbox_fail(1, 0); while(1); }
    IWDG->CTLR = 0xAAAA;
    ael_mailbox_pass();
    uint32_t tick = 0;
    while (1) {
        IWDG->CTLR = 0xAAAA;
        for (volatile int i = 0; i < 96000; i++);
        AEL_MAILBOX->detail0 = ++tick;
    }
}
