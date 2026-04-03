#include <stdint.h>

/* AEL mailbox — placed at 0x20007F00 (default address in ael_mailbox.h) */
#include "../ael_mailbox.h"

int main(void) {
    ael_mailbox_init();
    ael_mailbox_pass();
    while (1) {}
    return 0;
}
