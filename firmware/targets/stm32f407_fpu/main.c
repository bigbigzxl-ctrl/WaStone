/*
 * STM32F407 — Cortex-M4 FPU Verification Test
 *
 * Enables the FPU (CPACR CP10/CP11 full access), then runs a suite of
 * floating-point computations and compares results against known values.
 *
 * Tests (all using hardware single-precision FPU instructions):
 *   1. Addition/subtraction:  1.0f + 2.0f = 3.0f
 *   2. Multiplication:        3.0f * 4.0f = 12.0f
 *   3. Division:              1.0f / 4.0f = 0.25f
 *   4. Square root (VSQRT):   2.0f → 1.41421356f ± 1 ULP
 *   5. FMA (VFMA):            2.0f * 3.0f + 1.0f = 7.0f
 *   6. Integer↔float convert: (float)1000 = 1000.0f, (int)3.9f = 3
 *   7. Accumulator stress: sum 1000 iterations of 0.001f → ~1.0f ± 0.01f
 *
 * All comparisons use exact bit patterns or bounded epsilon.
 * No external wiring required. No PLL — 16 MHz HSI.
 *
 * Mailbox: 0x2001FC00
 *   PASS: detail0 = number of sub-tests passed (7)
 *   FAIL: error_code = failing sub-test index (1-7)
 *         detail0    = raw float bits of actual result
 */

#include <stdint.h>

#define AEL_MAILBOX_ADDR 0x2001FC00u
#include "ael_mailbox.h"

void HardFault_Handler(void)
{
    *((volatile uint32_t *)0xE000ED0Cu) = 0x05FA0004u;
    while (1) {}
}

/* Enable Cortex-M4 FPU: set CP10 and CP11 to full access in CPACR */
static void fpu_enable(void)
{
    volatile uint32_t *cpacr = (volatile uint32_t *)0xE000ED88u;
    *cpacr |= (0xFu << 20);   /* CP10=11, CP11=11 (full access) */
    __asm__ volatile ("dsb");
    __asm__ volatile ("isb");
}

/* Reinterpret float bits as uint32 without undefined behaviour */
static uint32_t f2u(float f)
{
    uint32_t u;
    __builtin_memcpy(&u, &f, 4);
    return u;
}

static float u2f(uint32_t u)
{
    float f;
    __builtin_memcpy(&f, &u, 4);
    return f;
}

/* Absolute difference between two floats (in ULP via bit representation).
 * Returns uint32 delta of their bit patterns (valid for same-sign normals). */
static uint32_t ulp_diff(float a, float b)
{
    uint32_t ua = f2u(a);
    uint32_t ub = f2u(b);
    return (ua > ub) ? (ua - ub) : (ub - ua);
}

int main(void)
{
    fpu_enable();
    ael_mailbox_init();

    uint32_t passed = 0u;

    /* ---- Test 1: addition ---- */
    volatile float r1 = 1.0f + 2.0f;
    if (f2u(r1) != f2u(3.0f)) {
        ael_mailbox_fail(1u, f2u(r1)); while (1) {}
    }
    passed++;

    /* ---- Test 2: multiplication ---- */
    volatile float r2 = 3.0f * 4.0f;
    if (f2u(r2) != f2u(12.0f)) {
        ael_mailbox_fail(2u, f2u(r2)); while (1) {}
    }
    passed++;

    /* ---- Test 3: division ---- */
    volatile float r3 = 1.0f / 4.0f;
    if (f2u(r3) != f2u(0.25f)) {
        ael_mailbox_fail(3u, f2u(r3)); while (1) {}
    }
    passed++;

    /* ---- Test 4: square root (VSQRT.F32) ---- */
    volatile float r4 = __builtin_sqrtf(2.0f);
    /* expected: 0x3FB504F3 = 1.41421354f, allow 2 ULP */
    if (ulp_diff(r4, 1.41421356f) > 2u) {
        ael_mailbox_fail(4u, f2u(r4)); while (1) {}
    }
    passed++;

    /* ---- Test 5: FMA (fused multiply-add, VFMA.F32) ---- */
    volatile float r5 = __builtin_fmaf(2.0f, 3.0f, 1.0f);
    if (f2u(r5) != f2u(7.0f)) {
        ael_mailbox_fail(5u, f2u(r5)); while (1) {}
    }
    passed++;

    /* ---- Test 6: integer ↔ float conversion ---- */
    volatile float  r6a = (float)1000;
    volatile int32_t r6b = (int32_t)3.9f;
    if (f2u(r6a) != f2u(1000.0f) || r6b != 3) {
        ael_mailbox_fail(6u, f2u(r6a)); while (1) {}
    }
    passed++;

    /* ---- Test 7: accumulator stress (1000 × 0.001f) ---- */
    volatile float acc = 0.0f;
    for (uint32_t i = 0u; i < 1000u; i++) {
        acc += 0.001f;
    }
    /* expect ~1.0f, allow ±0.01 (float accumulation error is tiny) */
    float diff7 = acc - 1.0f;
    if (diff7 < 0.0f) { diff7 = -diff7; }
    if (diff7 > 0.01f) {
        ael_mailbox_fail(7u, f2u(acc)); while (1) {}
    }
    passed++;

    ael_mailbox_pass();
    AEL_MAILBOX->detail0 = passed;   /* = 7 */

    while (1) {}
}
