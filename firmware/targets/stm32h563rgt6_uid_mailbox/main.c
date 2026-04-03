/*
 * stm32h563rgt6_uid_mailbox
 *
 * Reads the 96-bit unique device ID (UID) from the system flash
 * information block at 0x08FFF800 (3 × 32-bit words).
 * PASS if all three words are non-zero (erased device would read 0xFFFFFFFF,
 * which we also treat as invalid).
 *
 * detail0 = UID word0
 * The full UID is written to DIAG_BASE (0x20007EC0) for post-run inspection.
 *
 * FAIL codes:
 *   0xE001 — UID word 0 is 0x00000000 or 0xFFFFFFFF
 *   0xE002 — UID word 1 is 0x00000000 or 0xFFFFFFFF
 *   0xE003 — UID word 2 is 0x00000000 or 0xFFFFFFFF
 */

#include <stdint.h>
#include "../ael_mailbox.h"

#define UID_BASE        0x08FFF800u

#define DIAG_BASE       0x20007EC0u
#define DIAG(n)         (*(volatile uint32_t *)(DIAG_BASE + (n)*4u))

int main(void)
{
    ael_mailbox_init();

    uint32_t uid0 = *(volatile uint32_t *)(UID_BASE + 0x00u);
    uint32_t uid1 = *(volatile uint32_t *)(UID_BASE + 0x04u);
    uint32_t uid2 = *(volatile uint32_t *)(UID_BASE + 0x08u);

    DIAG(0) = uid0;
    DIAG(1) = uid1;
    DIAG(2) = uid2;

    if (uid0 == 0u || uid0 == 0xFFFFFFFFu) {
        ael_mailbox_fail(0xE001u, uid0);
        while (1) {}
    }
    if (uid1 == 0u || uid1 == 0xFFFFFFFFu) {
        ael_mailbox_fail(0xE002u, uid1);
        while (1) {}
    }
    if (uid2 == 0u || uid2 == 0xFFFFFFFFu) {
        ael_mailbox_fail(0xE003u, uid2);
        while (1) {}
    }

    AEL_MAILBOX->detail0 = uid0;
    ael_mailbox_pass();
    while (1) {}
    return 0;
}
