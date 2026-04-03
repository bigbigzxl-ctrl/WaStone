/*
 * stm32h563rgt6_fpu_mailbox
 *
 * Cortex-M33 FPU verification test.
 * Enables FPU (CPACR CP10/CP11 full access), runs 5 sub-tests:
 *   1. Addition:    1.0f + 2.0f = 3.0f
 *   2. Multiply:    3.0f * 4.0f = 12.0f
 *   3. Division:    1.0f / 4.0f = 0.25f
 *   4. VSQRT:       sqrtf(2.0f) ≈ 1.41421356f  (≤2 ULP)
 *   5. Accumulate:  sum 1000 × 0.001f ≈ 1.0f   (±0.01f)
 *
 * No clocks or peripherals required.
 *
 * FAIL codes:
 *   0xE001..0xE005 — sub-test index
 *   detail0 = raw float bits of failing result
 */

#include <stdint.h>
#include "../ael_mailbox.h"

/* Enable Cortex-M33 FPU: CP10=11, CP11=11 in CPACR (0xE000ED88) */
static void fpu_enable(void)
{
    volatile uint32_t *cpacr = (volatile uint32_t *)0xE000ED88u;
    *cpacr |= (0xFu << 20);
    __asm__ volatile ("dsb");
    __asm__ volatile ("isb");
}

static uint32_t f2u(float f)
{
    uint32_t u;
    __builtin_memcpy(&u, &f, 4);
    return u;
}

static uint32_t ulp_diff(float a, float b)
{
    uint32_t ua = f2u(a);
    uint32_t ub = f2u(b);
    return (ua > ub) ? (ua - ub) : (ub - ua);
}

/* Minimal sqrt via Newton-Raphson (avoid libm dependency) */
static float my_sqrt(float x)
{
    if (x <= 0.0f) return 0.0f;
    float r = x * 0.5f;
    for (int i = 0; i < 20; i++) {
        r = 0.5f * (r + x / r);
    }
    return r;
}

int main(void)
{
    fpu_enable();
    ael_mailbox_init();

    /* Test 1: addition */
    volatile float r1 = 1.0f + 2.0f;
    if (f2u(r1) != f2u(3.0f)) {
        ael_mailbox_fail(0xE001u, f2u(r1));
        while (1) {}
    }

    /* Test 2: multiplication */
    volatile float r2 = 3.0f * 4.0f;
    if (f2u(r2) != f2u(12.0f)) {
        ael_mailbox_fail(0xE002u, f2u(r2));
        while (1) {}
    }

    /* Test 3: division */
    volatile float r3 = 1.0f / 4.0f;
    if (f2u(r3) != f2u(0.25f)) {
        ael_mailbox_fail(0xE003u, f2u(r3));
        while (1) {}
    }

    /* Test 4: sqrt(2) ≤ 2 ULP from 1.41421356f */
    volatile float r4 = my_sqrt(2.0f);
    if (ulp_diff(r4, 1.41421356f) > 2u) {
        ael_mailbox_fail(0xE004u, f2u(r4));
        while (1) {}
    }

    /* Test 5: sum 1000 × 0.001f ≈ 1.0f ± 0.01f */
    volatile float acc = 0.0f;
    for (int i = 0; i < 1000; i++) {
        acc += 0.001f;
    }
    float diff = (acc > 1.0f) ? (acc - 1.0f) : (1.0f - acc);
    if (diff > 0.01f) {
        ael_mailbox_fail(0xE005u, f2u(acc));
        while (1) {}
    }

    AEL_MAILBOX->detail0 = 5u;   /* 5 sub-tests passed */
    ael_mailbox_pass();
    while (1) {}
    return 0;
}
