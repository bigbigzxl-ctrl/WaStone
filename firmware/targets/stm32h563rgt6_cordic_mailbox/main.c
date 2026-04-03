/*
 * stm32h563rgt6_cordic_mailbox
 *
 * STM32H563 CORDIC co-processor test.
 * CORDIC clock: AHB1 bit14 (CORDICEN).
 * CORDIC base:  0x40023800 (RM0481 SVD, differs from STM32U585!).
 *
 * Computes cosine in Q1.31 format:
 *   Test A: cos(0)   → expect 0x7FFFFFFF  (1.0)
 *   Test B: cos(π/3) → expect ≈ 0x40000000 (0.5), tolerance ±0x10000
 *
 * CORDIC angle Q1.31: maps [-π, π] → [-1, 1]
 *   cos(0):   angle = 0     → WDATA = 0x00000000
 *   cos(π/3): angle = 1/3   → WDATA = 0x2AAAAAAB
 *
 * FAIL codes:
 *   0xE001 — RRDY timeout (test A)
 *   0xE002 — cos(0) result < 0x7FFE0000
 *   0xE003 — RRDY timeout (test B)
 *   0xE004 — cos(π/3) error > 0x10000
 */

#include <stdint.h>
#include "../ael_mailbox.h"

/* RCC */
#define RCC_BASE        0x44020C00u
#define RCC_AHB1ENR     (*(volatile uint32_t *)(RCC_BASE + 0x088u))

/* CORDIC (AHB1 bit14) — base 0x40023800 for STM32H563 */
#define CORDIC_BASE     0x40023800u
#define CORDIC_CSR      (*(volatile uint32_t *)(CORDIC_BASE + 0x00u))
#define CORDIC_WDATA    (*(volatile uint32_t *)(CORDIC_BASE + 0x04u))
#define CORDIC_RDATA    (*(volatile uint32_t *)(CORDIC_BASE + 0x08u))

#define CORDIC_CSR_RRDY  (1u << 31)

/* CSR: FUNC=cosine(0), PRECISION=6(24 iters), SCALE=0, NRES=0, NARGS=0,
 *      RESSIZE=0(32-bit), ARGSIZE=0(32-bit) */
#define CORDIC_CFG_COS32  \
    ((0u <<  0) | /* FUNC=cosine  */ \
     (6u <<  4) | /* PRECISION=6  */ \
     (0u <<  8) | /* SCALE=0      */ \
     (0u << 19) | /* NRES=1       */ \
     (0u << 20) | /* NARGS=1      */ \
     (0u << 21) | /* RESSIZE=32b  */ \
     (0u << 22))  /* ARGSIZE=32b  */

#define TIMEOUT  1000000u

int main(void)
{
    ael_mailbox_init();

    /* Enable CORDIC clock (AHB1 bit14) */
    RCC_AHB1ENR |= (1u << 14);
    (void)RCC_AHB1ENR;

    /* Configure CORDIC: cosine, 32-bit Q1.31, precision 6 */
    CORDIC_CSR = CORDIC_CFG_COS32;

    /* ---- Test A: cos(0) = 1.0 ---- */
    CORDIC_WDATA = 0x00000000u;
    {
        uint32_t t;
        for (t = 0; t < TIMEOUT; t++) {
            if (CORDIC_CSR & CORDIC_CSR_RRDY) break;
        }
        if (!(CORDIC_CSR & CORDIC_CSR_RRDY)) {
            ael_mailbox_fail(0xE001u, 0u);
            while (1) {}
        }
    }
    uint32_t result_a = CORDIC_RDATA;
    if (result_a < 0x7FFE0000u) {
        ael_mailbox_fail(0xE002u, result_a);
        while (1) {}
    }

    /* ---- Test B: cos(π/3) ≈ 0.5 ---- */
    CORDIC_WDATA = 0x2AAAAAAAu;
    {
        uint32_t t;
        for (t = 0; t < TIMEOUT; t++) {
            if (CORDIC_CSR & CORDIC_CSR_RRDY) break;
        }
        if (!(CORDIC_CSR & CORDIC_CSR_RRDY)) {
            ael_mailbox_fail(0xE003u, 0u);
            while (1) {}
        }
    }
    uint32_t result_b = CORDIC_RDATA;
    uint32_t expected = 0x40000000u;
    uint32_t diff = (result_b >= expected) ? (result_b - expected) : (expected - result_b);
    if (diff > 0x10000u) {
        ael_mailbox_fail(0xE004u, result_b);
        while (1) {}
    }

    AEL_MAILBOX->detail0 = result_b;
    ael_mailbox_pass();
    while (1) {}
    return 0;
}
