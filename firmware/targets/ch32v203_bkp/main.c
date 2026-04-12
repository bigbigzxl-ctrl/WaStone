/* ch32v203_bkp — BKP backup register read/write test
 *
 * Writes 4 test patterns to BKP_DR1..DR4, reads them back, verifies.
 * BKP domain access requires PWR_BackupAccessCmd(ENABLE).
 *
 * Stage 0: no wiring required.
 * detail0 on PASS: 0 (all 4 registers verified OK).
 */
#define AEL_MAILBOX_ADDR 0x20000600u
#include "ael_mailbox.h"
#include "ch32v20x.h"

static const uint16_t patterns[4] = { 0xA55A, 0x1234, 0xDEAD, 0xBEEF };

int main(void)
{
    ael_mailbox_init();

    /* Enable PWR and BKP clocks, allow backup domain write access */
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_PWR | RCC_APB1Periph_BKP, ENABLE);
    PWR_BackupAccessCmd(ENABLE);

    BKP_ClearFlag();

    /* Write test patterns */
    BKP_WriteBackupRegister(BKP_DR1, patterns[0]);
    BKP_WriteBackupRegister(BKP_DR2, patterns[1]);
    BKP_WriteBackupRegister(BKP_DR3, patterns[2]);
    BKP_WriteBackupRegister(BKP_DR4, patterns[3]);

    /* Read back and compare */
    uint32_t err = 0;
    if (BKP_ReadBackupRegister(BKP_DR1) != patterns[0]) err |= 1u;
    if (BKP_ReadBackupRegister(BKP_DR2) != patterns[1]) err |= 2u;
    if (BKP_ReadBackupRegister(BKP_DR3) != patterns[2]) err |= 4u;
    if (BKP_ReadBackupRegister(BKP_DR4) != patterns[3]) err |= 8u;

    if (err) {
        ael_mailbox_fail(1, err);
    } else {
        ael_mailbox_pass();
        AEL_MAILBOX->detail0 = 0;
    }

    uint32_t tick = 0;
    while (1) {
        AEL_MAILBOX->detail0 = ++tick;
        for (volatile int i = 0; i < 720000; i++);
    }
}
