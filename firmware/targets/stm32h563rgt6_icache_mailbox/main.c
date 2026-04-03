/*
 * stm32h563rgt6_icache_mailbox — ICACHE + DCACHE smoke test
 *
 * ICACHE (instruction cache for internal flash):
 *   BASE = 0x40030400 (AHB1 + 0x10400), no RCC clock gate needed
 *   CR: bit0=EN, bit1=CACHEINV, bit16=HITMEN, bit17=MISSMEN, bit18=HITMRST, bit19=MISSMRST
 *   SR: bit0=BUSYF, bit1=BSYENDF, bit2=ERRF
 *   HMONR: hit monitor count
 *
 * DCACHE1 (data cache, used for cacheable regions):
 *   BASE = 0x40031400 (AHB1 + 0x11400), RCC AHB1ENR bit30
 *   CR: bit0=EN, bit1=CACHEINV
 *   SR: bit0=BUSYF, bit1=BSYENDF, bit2=ERRF
 *
 * Test:
 *   1. Enable ICACHE, enable hit monitor, reset counter
 *   2. Execute a code loop (forces instruction fetches → cache hits)
 *   3. Read HMONR → must be > 0
 *   4. Perform ICACHE full invalidation, wait BSYENDF, check no error
 *   5. Enable DCACHE1 clock, enable DCACHE, invalidate, check no error
 *
 * FAIL codes:
 *   0xE001 — ICACHE hit count == 0 after loop (cache not functioning)
 *   0xE002 — ICACHE invalidation error (ERRF set)
 *   0xE003 — DCACHE invalidation timeout
 *   0xE004 — DCACHE error (ERRF set)
 */

#include <stdint.h>
#include "../ael_mailbox.h"

#define RCC_BASE        0x44020C00u
#define RCC_AHB1ENR     (*(volatile uint32_t *)(RCC_BASE + 0x088u))

/* ICACHE — no dedicated RCC clock gate (always accessible) */
#define ICACHE_BASE     0x40030400u
#define ICACHE_CR       (*(volatile uint32_t *)(ICACHE_BASE + 0x00u))
#define ICACHE_SR       (*(volatile uint32_t *)(ICACHE_BASE + 0x04u))
#define ICACHE_FCR      (*(volatile uint32_t *)(ICACHE_BASE + 0x0Cu))
#define ICACHE_HMONR    (*(volatile uint32_t *)(ICACHE_BASE + 0x10u))
#define ICACHE_MMONR    (*(volatile uint32_t *)(ICACHE_BASE + 0x14u))

#define ICACHE_CR_EN        (1u << 0)
#define ICACHE_CR_CACHEINV  (1u << 1)
#define ICACHE_CR_HITMEN    (1u << 16)
#define ICACHE_CR_MISSMEN   (1u << 17)
#define ICACHE_CR_HITMRST   (1u << 18)
#define ICACHE_CR_MISSMRST  (1u << 19)
#define ICACHE_SR_BUSYF     (1u << 0)
#define ICACHE_SR_BSYENDF   (1u << 1)
#define ICACHE_SR_ERRF      (1u << 2)
#define ICACHE_FCR_CBSYENDF (1u << 1)

/* DCACHE1 — RCC AHB1ENR bit30 */
#define DCACHE1_BASE    0x40031400u
#define DCACHE_CR       (*(volatile uint32_t *)(DCACHE1_BASE + 0x00u))
#define DCACHE_SR       (*(volatile uint32_t *)(DCACHE1_BASE + 0x04u))
#define DCACHE_FCR      (*(volatile uint32_t *)(DCACHE1_BASE + 0x0Cu))

#define DCACHE_CR_EN        (1u << 0)
#define DCACHE_CR_CACHEINV  (1u << 1)
#define DCACHE_SR_BUSYF     (1u << 0)
#define DCACHE_SR_BSYENDF   (1u << 1)
#define DCACHE_SR_ERRF      (1u << 2)
#define DCACHE_FCR_CBSYENDF (1u << 1)

#define TIMEOUT  1000000u

/* Dummy loop that forces many sequential instruction fetches from flash */
static volatile uint32_t dummy_sink;
static void run_icache_workload(void)
{
    volatile uint32_t acc = 0u;
    for (uint32_t i = 0u; i < 512u; i++) {
        acc += i * 3u + 1u;
    }
    dummy_sink = acc;
}

int main(void)
{
    ael_mailbox_init();

    /* ── ICACHE ─────────────────────────────────────────────── */
    /* 1. Enable ICACHE + hit/miss monitors, reset counters */
    ICACHE_CR = ICACHE_CR_EN | ICACHE_CR_HITMEN | ICACHE_CR_MISSMEN
              | ICACHE_CR_HITMRST | ICACHE_CR_MISSMRST;
    (void)ICACHE_CR;
    /* Clear HITMRST/MISSMRST after reset (self-clearing, but be explicit) */
    ICACHE_CR = ICACHE_CR_EN | ICACHE_CR_HITMEN | ICACHE_CR_MISSMEN;
    (void)ICACHE_CR;

    /* 2. Execute instruction-fetch workload */
    run_icache_workload();
    run_icache_workload();
    run_icache_workload();

    /* 3. Check hit count */
    uint32_t hits = ICACHE_HMONR;
    if (hits == 0u) {
        ael_mailbox_fail(0xE001u, hits);
        while (1) {}
    }

    /* 4. Full invalidation: set CACHEINV, wait for BSYENDF, check no error */
    ICACHE_FCR = ICACHE_FCR_CBSYENDF;          /* clear previous BSYENDF */
    ICACHE_CR  = ICACHE_CR_EN | ICACHE_CR_CACHEINV;
    uint32_t t;
    for (t = 0u; t < TIMEOUT; t++) {
        if (ICACHE_SR & ICACHE_SR_BSYENDF) break;
    }
    if (ICACHE_SR & ICACHE_SR_ERRF) {
        ael_mailbox_fail(0xE002u, ICACHE_SR);
        while (1) {}
    }

    /* ── DCACHE ─────────────────────────────────────────────── */
    /* 5. Enable DCACHE1 clock (AHB1ENR bit30) */
    RCC_AHB1ENR |= (1u << 30);
    (void)RCC_AHB1ENR;

    /* 6. Enable DCACHE */
    DCACHE_CR = DCACHE_CR_EN;
    (void)DCACHE_CR;

    /* 7. Full invalidation */
    DCACHE_FCR = DCACHE_FCR_CBSYENDF;
    DCACHE_CR  = DCACHE_CR_EN | DCACHE_CR_CACHEINV;
    for (t = 0u; t < TIMEOUT; t++) {
        if (DCACHE_SR & DCACHE_SR_BSYENDF) break;
    }
    if (t == TIMEOUT) {
        ael_mailbox_fail(0xE003u, DCACHE_SR);
        while (1) {}
    }
    if (DCACHE_SR & DCACHE_SR_ERRF) {
        ael_mailbox_fail(0xE004u, DCACHE_SR);
        while (1) {}
    }

    AEL_MAILBOX->detail0 = hits;
    ael_mailbox_pass();
    while (1) {}
    return 0;
}
