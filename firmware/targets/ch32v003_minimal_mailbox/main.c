/*
 * ch32v003_minimal_mailbox — AEL zero-wiring mailbox test for CH32V003
 *
 * Writes the AEL mailbox header to a fixed SRAM region:
 *   magic  = 0xAE100001  (AEL_MAILBOX_MAGIC)
 *   status = 0x00000002  (AEL_STATUS_PASS)
 *
 * CH32V003 SRAM: 2 KB at 0x20000000–0x200007FF
 * Stack:  256 B at top  (0x20000700–0x200007FF)
 * Mailbox: 16 B at 0x20000600  (safe below stack)
 *
 * No peripherals configured — the test is purely memory-write.
 * Host (wch-openocd GDB) halts the MCU and reads 0x20000600 to verify.
 */

#define AEL_MAILBOX_ADDR  0x20000600u
#include "ael_mailbox.h"
#include "ch32v003fun.h"

/* Required by ch32v003fun.c startup — called before main (not used here) */
void SystemInit(void) {}

int main(void)
{
    ael_mailbox_init();
    ael_mailbox_pass();

    /* Spin forever — GDB reads mailbox while MCU is running */
    while (1) {
        asm volatile("nop");
    }

    return 0;
}
