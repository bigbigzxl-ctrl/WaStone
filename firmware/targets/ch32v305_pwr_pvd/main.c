/* ch32v305_pwr_pvd — PWR PVD voltage detector self-test
 *
 * PVD threshold MODE7 ≈ 2.9V. At 3.3V supply, PVDO flag = 0 (above threshold).
 * PVDO == 0 → PASS; PVDO == 1 → FAIL.
 * detail0 on PASS: PWR->CSR raw value.
 */
#define AEL_MAILBOX_ADDR 0x20000600u
#include "ael_mailbox.h"
#include "ch32v30x.h"

int main(void)
{
    ael_mailbox_init();

    RCC_APB1PeriphClockCmd(RCC_APB1Periph_PWR, ENABLE);

    PWR_PVDLevelConfig(PWR_PVDLevel_2V9);
    PWR_PVDCmd(ENABLE);

    for (volatile uint32_t i = 0; i < 96000; i++) __asm__("nop");

    FlagStatus pvdo = PWR_GetFlagStatus(PWR_FLAG_PVDO);
    uint32_t csr    = PWR->CSR;

    if (pvdo != RESET) {
        ael_mailbox_fail(1, csr);
    } else {
        ael_mailbox_pass();
        AEL_MAILBOX->detail0 = csr;
    }

    uint32_t tick = 0;
    while (1) {
        AEL_MAILBOX->detail0 = ++tick;
        for (volatile int i = 0; i < 960000; i++);
    }
}
