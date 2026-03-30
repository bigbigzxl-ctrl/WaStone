#include <stdint.h>
#include "../ael_mailbox.h"

int main(void) {
    ael_mailbox_init();
    ael_mailbox_pass();
    while (1) {}
    return 0;
}
