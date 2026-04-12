/*
 * ch32v203_minimal_mailbox — AEL zero-wiring mailbox test for CH32V203
 *
 * Writes AEL mailbox at 0x20000600 (SRAM offset 0x600, well within 20KB).
 * Host (wch-openocd GDB) halts MCU and reads 0x20000600 to verify PASS.
 *
 * CH32V203 SRAM: 20 KB at 0x20000000–0x20004FFF
 * Mailbox: 16 B at 0x20000600
 */

#define AEL_MAILBOX_ADDR  0x20000600u
#include "ael_mailbox.h"
#include "ch32v20x.h"

int main(void)
{
    ael_mailbox_init();
    ael_mailbox_pass();

    /* Spin — GDB reads mailbox while MCU is running */
    while (1) {
        __asm__("nop");
    }
    return 0;
}
