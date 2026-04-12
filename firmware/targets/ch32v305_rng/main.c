/* ch32v305_rng — Hardware RNG self-test (CH32V305/307 specific)
 * Board: CH32V305RBT6
 * No wiring required (Stage 1).
 *
 * Reads 8 random 32-bit values. Verifies:
 *   - No value is 0x00000000 or 0xFFFFFFFF (degenerate)
 *   - Not all values are identical (stuck output)
 * detail0 on PASS: last random value read.
 */
#define AEL_MAILBOX_ADDR 0x20000600u
#include "ael_mailbox.h"
#include "ch32v30x.h"
#include "ch32v30x_rng.h"

#define N_SAMPLES 8

int main(void)
{
    ael_mailbox_init();

    /* Enable RNG clock and peripheral */
    RCC_AHBPeriphClockCmd(RCC_AHBPeriph_RNG, ENABLE);
    RNG_Cmd(ENABLE);

    /* Wait for first DRDY */
    uint32_t t = 500000;
    while (!RNG_GetFlagStatus(RNG_FLAG_DRDY) && --t);
    if (!t) { ael_mailbox_fail(1, 0); while (1); }

    uint32_t samples[N_SAMPLES];
    for (int i = 0; i < N_SAMPLES; i++) {
        /* Wait for next DRDY */
        t = 200000;
        while (!RNG_GetFlagStatus(RNG_FLAG_DRDY) && --t);
        if (!t) { ael_mailbox_fail(2, (uint32_t)i); while (1); }
        samples[i] = RNG_GetRandomNumber();
    }

    /* Check: no degenerate values */
    for (int i = 0; i < N_SAMPLES; i++) {
        if (samples[i] == 0x00000000u || samples[i] == 0xFFFFFFFFu) {
            ael_mailbox_fail(3, samples[i]);
            while (1);
        }
    }

    /* Check: not all identical (stuck output) */
    uint32_t all_same = 1;
    for (int i = 1; i < N_SAMPLES; i++) {
        if (samples[i] != samples[0]) { all_same = 0; break; }
    }
    if (all_same) {
        ael_mailbox_fail(4, samples[0]);
        while (1);
    }

    ael_mailbox_pass();
    AEL_MAILBOX->detail0 = samples[N_SAMPLES - 1];

    uint32_t tick = 0;
    while (1) {
        AEL_MAILBOX->detail0 = ++tick;
        for (volatile int i = 0; i < 960000; i++);
    }
}
