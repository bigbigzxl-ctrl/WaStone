/*
 * stm32h563rgt6_dbgmcu_mailbox — DBGMCU device ID self-test
 *
 * Reads the Debug MCU identification registers. DBGMCU is always accessible
 * regardless of clock state. No peripheral clock enable required.
 *
 * DBGMCU_BASE = 0x44024000
 *
 * IDCODE register (+0x00):
 *   bits[11:0]  = DEV_ID   (device identifier)
 *   bits[31:16] = REV_ID   (revision identifier)
 *
 * Expected DEV_ID for STM32H563 = 0x484
 * REV_ID: 0x1000 = Rev A, 0x1001 = Rev Z, 0x2000 = Rev B (varies by silicon cut)
 *
 * FAIL codes:
 *   0xE001 — IDCODE reads 0x00000000 (DBGMCU not accessible)
 *   0xE002 — DEV_ID mismatch (not 0x484 — wrong device or address wrong)
 *
 * detail0: IDCODE register value [REV_ID[31:16] | DEV_ID[11:0]]
 */

#include <stdint.h>
#include "../ael_mailbox.h"

#define DBGMCU_BASE     0x44024000u
#define DBGMCU_IDCODE   (*(volatile uint32_t *)(DBGMCU_BASE + 0x000u))
#define DBGMCU_CR       (*(volatile uint32_t *)(DBGMCU_BASE + 0x004u))

#define STM32H563_DEV_ID  0x484u

int main(void)
{
    ael_mailbox_init();

    /* Read IDCODE — no clock enable needed, always accessible */
    uint32_t idcode = DBGMCU_IDCODE;

    if (idcode == 0u) {
        ael_mailbox_fail(0xE001u, idcode);
        while (1) {}
    }

    uint32_t dev_id = idcode & 0xFFFu;
    if (dev_id != STM32H563_DEV_ID) {
        ael_mailbox_fail(0xE002u, idcode);
        while (1) {}
    }

    /* detail0 = full IDCODE (REV_ID in upper 16 bits, DEV_ID in lower 12) */
    AEL_MAILBOX->detail0 = idcode;
    ael_mailbox_pass();
    while (1) {}
    return 0;
}
