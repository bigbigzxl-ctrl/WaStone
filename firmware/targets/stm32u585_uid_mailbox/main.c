#include <stdint.h>
#include "../ael_mailbox.h"

#define UID_BASE  0x0BFA0700u
#define UID_W0    (*(volatile uint32_t *)(UID_BASE + 0x00u))
#define UID_W1    (*(volatile uint32_t *)(UID_BASE + 0x04u))
#define UID_W2    (*(volatile uint32_t *)(UID_BASE + 0x08u))

int main(void) {
    ael_mailbox_init();
    uint32_t w0 = UID_W0;
    uint32_t w1 = UID_W1;
    uint32_t w2 = UID_W2;
    /* At least one word must be non-zero */
    if ((w0 | w1 | w2) != 0u) {
        AEL_MAILBOX->detail0 = w0;
        ael_mailbox_pass();
    } else {
        ael_mailbox_fail(0xE001u, 0u);
    }
    while (1) {}
    return 0;
}
