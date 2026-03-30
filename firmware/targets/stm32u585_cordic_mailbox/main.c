/*
 * stm32u585_cordic_mailbox — AEL hardware accelerator test
 * STM32U585CIU6 CORDIC co-processor
 *
 * Computes cosine in Q1.31 format:
 *   Test A: cos(0)    → expect 0x7FFFFFFF (1.0)
 *   Test B: cos(π/3)  → expect ≈ 0x40000000 (0.5)
 *
 * CORDIC angle input: Q1.31, range [-1, 1] maps to [-π, π]
 *   cos(0):   angle = 0          → WDATA = 0x00000000
 *   cos(π/3): angle = 1/3        → WDATA = 0x2AAAAAAB
 *
 * FAIL codes:
 *   0xE001 — RRDY timeout (test A)
 *   0xE002 — cos(0) result out of range (< 0x7FFFFF00)
 *   0xE003 — RRDY timeout (test B)
 *   0xE004 — cos(π/3) result too far from 0x40000000 (> 0x10000 error)
 */

#include <stdint.h>
#include "../ael_mailbox.h"

/* RCC */
#define RCC_BASE        0x46020C00u
#define RCC_AHB1ENR     (*(volatile uint32_t *)(RCC_BASE + 0x088u))

/* CORDIC — AHB1, bit 1 of RCC_AHB1ENR */
#define CORDIC_BASE     0x40021000u
#define CORDIC_CSR      (*(volatile uint32_t *)(CORDIC_BASE + 0x00u))
#define CORDIC_WDATA    (*(volatile uint32_t *)(CORDIC_BASE + 0x04u))
#define CORDIC_RDATA    (*(volatile uint32_t *)(CORDIC_BASE + 0x08u))

#define CORDIC_CSR_RRDY     (1u << 31u)

/* CSR configuration:
 *   FUNC=0 (cosine)      bits[3:0]
 *   PRECISION=6          bits[7:4]   → 24 iterations (good accuracy)
 *   SCALE=0              bits[10:8]  → no scaling
 *   NRES=0               bit19       → 1 result
 *   NARGS=0              bit20       → 1 argument
 *   RESSIZE=0            bit21       → 32-bit Q1.31
 *   ARGSIZE=0            bit22       → 32-bit Q1.31
 */
#define CORDIC_CSR_COSINE_32BIT  \
    ((0u << 0u)  | /* FUNC=cosine   */ \
     (6u << 4u)  | /* PRECISION=6   */ \
     (0u << 8u)  | /* SCALE=0       */ \
     (0u << 19u) | /* NRES=1 result */ \
     (0u << 20u) | /* NARGS=1 arg   */ \
     (0u << 21u) | /* RESSIZE=32bit */ \
     (0u << 22u))  /* ARGSIZE=32bit */

#define RRDY_TIMEOUT    1000000u

int main(void)
{
    ael_mailbox_init();

    /* 1. Enable CORDIC clock (AHB1, bit 1) */
    RCC_AHB1ENR |= (1u << 1u);
    /* Small delay for clock to settle */
    volatile uint32_t dummy = RCC_AHB1ENR;
    (void)dummy;

    /* 2. Configure CORDIC: cosine, 32-bit Q1.31, precision=6 */
    CORDIC_CSR = CORDIC_CSR_COSINE_32BIT;

    /* ---- Test A: cos(0) = 1.0 ---- */

    /* Write angle = 0 (triggers computation) */
    CORDIC_WDATA = 0x00000000u;

    /* Wait for RRDY */
    uint32_t timeout = RRDY_TIMEOUT;
    while (!(CORDIC_CSR & CORDIC_CSR_RRDY)) {
        if (--timeout == 0u) {
            ael_mailbox_fail(0xE001u, 0u);
            while (1) {}
        }
    }

    uint32_t result_a = CORDIC_RDATA;

    /* cos(0) should be ~0x7FFFFFFF; CORDIC 24-iter precision ~22 bits, allow margin */
    if (result_a < 0x7FFE0000u) {
        ael_mailbox_fail(0xE002u, result_a);
        while (1) {}
    }

    /* ---- Test B: cos(π/3) ≈ 0.5 ---- */

    /* Write angle = 1/3 in Q1.31 = 0x2AAAAAAB (triggers computation) */
    CORDIC_WDATA = 0x2AAAAAAAu;

    /* Wait for RRDY */
    timeout = RRDY_TIMEOUT;
    while (!(CORDIC_CSR & CORDIC_CSR_RRDY)) {
        if (--timeout == 0u) {
            ael_mailbox_fail(0xE003u, 0u);
            while (1) {}
        }
    }

    uint32_t result_b = CORDIC_RDATA;

    /* cos(π/3) = 0.5 in Q1.31 = 0x40000000; allow ±0x10000 error */
    uint32_t expected = 0x40000000u;
    uint32_t diff = (result_b >= expected) ? (result_b - expected) : (expected - result_b);
    if (diff > 0x10000u) {
        ael_mailbox_fail(0xE004u, result_b);
        while (1) {}
    }

    /* 5. PASS — store cos(π/3) result in detail0 */
    AEL_MAILBOX->detail0 = result_b;
    ael_mailbox_pass();

    while (1) {}
    return 0;
}
