/*
 * stm32h563rgt6_pka_mailbox — PKA arithmetic add self-test
 *
 * H563 PKA initialization behavior:
 *   - EN=1: bit1 fires immediately (erase started), no OPERRF yet
 *   - PKA scans RAM ECC asynchronously; OPERRF fires when bad ECC found
 *   - OPERRF is a sticky latch — clear it so the erase can continue scanning
 *   - Keep clearing OPERRF (without toggling EN) until INITOK (bit0) fires
 *
 * FAIL codes:
 *   0xE001 — INITOK timeout (detail0=SR at timeout)
 *   0xE002 — PROCENDF timeout (detail0=SR at timeout)
 *   0xE003 — error flag set after operation (detail0=SR)
 *   0xE004 — result mismatch (detail0=actual result)
 */

#include <stdint.h>
#include "../ael_mailbox.h"

#define RCC_BASE        0x44020C00u
#define RCC_AHB2ENR     (*(volatile uint32_t *)(RCC_BASE + 0x08Cu))

#define PKA_BASE        0x420C2000u
#define PKA_RAM_BASE    0x420C2400u
#define PKA_CR          (*(volatile uint32_t *)(PKA_BASE + 0x000u))
#define PKA_SR          (*(volatile uint32_t *)(PKA_BASE + 0x004u))
#define PKA_CLRFR       (*(volatile uint32_t *)(PKA_BASE + 0x008u))

#define PKA_CR_EN       (1u << 0)
#define PKA_CR_START    (1u << 1)
#define PKA_SR_INITOK   (1u << 0)
#define PKA_SR_PROCENDF (1u << 17)
#define PKA_SR_RAMERRF  (1u << 19)
#define PKA_SR_ADDRERRF (1u << 20)
#define PKA_SR_OPERRF   (1u << 21)

#define PKA_MODE_ARITHMETIC_ADD  (0x09u)
#define PKA_RAM_W(idx)  (*(volatile uint32_t *)(PKA_RAM_BASE + (idx) * 4u))

#define IDX_NB_BITS  2u
#define IDX_OP1      404u
#define IDX_OP2      546u
#define IDX_RESULT   670u

#define TIMEOUT         1000000u
#define INITOK_TIMEOUT  10000000u

int main(void)
{
    ael_mailbox_init();

    /* Enable PKA clock */
    RCC_AHB2ENR |= (1u << 19);
    (void)RCC_AHB2ENR;

    /* Enable PKA — triggers internal RAM erase (async ECC scan) */
    PKA_CR = PKA_CR_EN;
    (void)PKA_CR;

    /* Wait for INITOK. PKA scans RAM ECC asynchronously after EN=1.
     * OPERRF fires when a bad ECC word is encountered during the scan.
     * Clear OPERRF immediately to allow the scan to continue — do NOT
     * toggle EN (that would restart from the beginning each time). */
    uint32_t t;
    for (t = 0u; t < INITOK_TIMEOUT; t++) {
        uint32_t sr = PKA_SR;
        if (sr & PKA_SR_INITOK) break;
        if (sr & PKA_SR_OPERRF) {
            PKA_CLRFR = PKA_SR_OPERRF;
            (void)PKA_CLRFR;
            /* yield a few cycles for the erase to advance */
            (void)PKA_SR;
            (void)PKA_SR;
        }
    }
    if (t == INITOK_TIMEOUT) {
        ael_mailbox_fail(0xE001u, PKA_SR);
        while (1) {}
    }

    /* Clear residual flags */
    PKA_CLRFR = (1u << 21) | (1u << 20) | (1u << 19) | (1u << 17);
    (void)PKA_CLRFR;

    /* Write operands: NB_BITS=32, OP1=0x1234, OP2=0x5678, expect 0x68AC */
    PKA_RAM_W(IDX_NB_BITS)  = 32u;
    PKA_RAM_W(IDX_OP1 + 0)  = 0x00001234u;
    PKA_RAM_W(IDX_OP1 + 1)  = 0u;
    PKA_RAM_W(IDX_OP2 + 0)  = 0x00005678u;
    PKA_RAM_W(IDX_OP2 + 1)  = 0u;

    /* Set MODE and START */
    {
        uint32_t cr = PKA_CR;
        cr = (cr & ~(0x3Fu << 8)) | (PKA_MODE_ARITHMETIC_ADD << 8);
        PKA_CR = cr;
        (void)PKA_CR;
        PKA_CR = cr | PKA_CR_START;
    }

    /* Wait for PROCENDF or any error flag */
    for (t = 0u; t < TIMEOUT; t++) {
        uint32_t sr = PKA_SR;
        if (sr & (PKA_SR_PROCENDF | PKA_SR_RAMERRF | PKA_SR_ADDRERRF | PKA_SR_OPERRF)) break;
    }
    if (t == TIMEOUT) {
        ael_mailbox_fail(0xE002u, PKA_SR);
        while (1) {}
    }

    uint32_t sr = PKA_SR;
    if (sr & (PKA_SR_RAMERRF | PKA_SR_ADDRERRF | PKA_SR_OPERRF)) {
        ael_mailbox_fail(0xE003u, sr);
        while (1) {}
    }

    uint32_t result = PKA_RAM_W(IDX_RESULT);
    if (result != 0x000068ACu) {
        ael_mailbox_fail(0xE004u, result);
        while (1) {}
    }

    AEL_MAILBOX->detail0 = result;

    /* Zero PKA RAM while EN=1 so next run starts with clean ECC state */
    {
        uint32_t i;
        for (i = 0u; i < 1334u; i++) {
            PKA_RAM_W(i) = 0u;
        }
    }

    ael_mailbox_pass();
    while (1) {}
    return 0;
}
