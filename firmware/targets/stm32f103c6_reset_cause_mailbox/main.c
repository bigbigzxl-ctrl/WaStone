#include <stdint.h>

#define AEL_MAILBOX_ADDR 0x20002400u
#include "../stm32f103c6/ael_mailbox.h"

#define RCC_BASE        0x40021000u
#define RCC_CSR         (*(volatile uint32_t *)(RCC_BASE + 0x24u))

#define RCC_CSR_RMVF    (1u << 24)
#define RESET_FLAGS_MASK (0xFC000000u)

#define ERR_NO_FLAGS    0x41u
#define ERR_CLEAR_FAIL  0x42u

int main(void) {
    uint32_t before;
    uint32_t after;

    ael_mailbox_init();

    before = RCC_CSR & RESET_FLAGS_MASK;
    if (before == 0u) {
        ael_mailbox_fail(ERR_NO_FLAGS, RCC_CSR);
        while (1) {}
    }

    RCC_CSR |= RCC_CSR_RMVF;
    after = RCC_CSR & RESET_FLAGS_MASK;
    if (after != 0u) {
        ael_mailbox_fail(ERR_CLEAR_FAIL, after);
        while (1) {}
    }

    AEL_MAILBOX->detail0 = before;
    ael_mailbox_pass();

    while (1) {}
}
