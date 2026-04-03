/*
 * stm32h563rgt6_hash_mailbox — HASH SHA-256 known-answer self-test
 *
 * Computes SHA-256 of "abc" (3 bytes) using the hardware HASH peripheral
 * and verifies the first 4 bytes of the digest against the known answer.
 *
 * Known answer: SHA-256("abc") = BA7816BF 8F01CFEA 414140DE 5DAE2EC7
 *                                 3B00361B BEF04693 48423923 960594D
 *
 * HASH (RM0481 §30):
 *   HASH_BASE   = AHB2 + 0xA0400 = 0x420C0400
 *   HASH_DIGEST = AHB2 + 0xA0710 = 0x420C0710
 *   RCC_AHB2ENR = RCC_BASE + 0x08C, bit17 = HASHEN
 *
 * HASH registers at HASH_BASE:
 *   CR   +0x00: INIT bit2, DATATYPE[5:4], MODE bit6, ALGO[20:17]
 *   DIN  +0x04: data input
 *   STR  +0x08: NBLW[4:0]=valid bits in last word, DCAL bit8=finalize
 *   SR   +0x24: DINIS bit0, BUSY bit3
 *
 * SHA-256 ALGO encoding: bits[20:17]=0b0100 → CR bit19=1 → 0x00080000
 * DATATYPE=0 (32-bit, big-endian, no byte swap needed)
 *
 * "abc" = 0x61 0x62 0x63 → DIN = 0x61626300 (3 bytes, 24 valid bits)
 * STR: NBLW=24 (bits), DCAL=1 → STR = 24 | (1<<8) = 0x118
 *
 * FAIL codes:
 *   0xE001 — HASH busy timeout
 *   0xE002 — digest mismatch, detail0 = actual digest[0]
 */

#include <stdint.h>
#include "../ael_mailbox.h"

#define RCC_BASE        0x44020C00u
#define RCC_AHB2ENR     (*(volatile uint32_t *)(RCC_BASE + 0x08Cu))

#define HASH_BASE       0x420C0400u
#define HASH_CR         (*(volatile uint32_t *)(HASH_BASE + 0x00u))
#define HASH_DIN        (*(volatile uint32_t *)(HASH_BASE + 0x04u))
#define HASH_STR        (*(volatile uint32_t *)(HASH_BASE + 0x08u))
#define HASH_SR         (*(volatile uint32_t *)(HASH_BASE + 0x24u))

#define HASH_DIGEST_BASE  0x420C0710u
#define HASH_HR(n)      (*(volatile uint32_t *)(HASH_DIGEST_BASE + (n)*4u))

/* CR bits */
#define HASH_CR_INIT      (1u << 2)
/* SHA-256: ALGO[3:0] = 0b0011 → ALGO_1(bit18) | ALGO_0(bit17) = 0x00060000 */
#define HASH_CR_SHA256    ((1u << 17) | (1u << 18))

/* STR bits */
#define HASH_STR_DCAL     (1u << 8)

/* SR bits */
#define HASH_SR_BUSY      (1u << 3)

#define TIMEOUT  1000000u

/* Expected SHA-256 of "abcd" (NBLW=0): first word — verified separately */
/* SHA-256("abcd") = 88d4266fd4e6338d13b845fcf289579d209c897823b9217da3e161936f031589 */
#define SHA256_ABCD_W0  0x88D4266Fu

int main(void)
{
    ael_mailbox_init();

    /* 1. Enable HASH clock (AHB2 bit17) */
    RCC_AHB2ENR |= (1u << 17);
    (void)RCC_AHB2ENR;

    /*
     * 2. Set ALGO first (without INIT), then set INIT.
     *    RM0481: "Write the algorithm field before setting INIT."
     *    SHA-256: ALGO[3:0]=0b0100 → ALGO_2=bit19 set
     */
    HASH_CR = HASH_CR_SHA256;
    HASH_CR |= HASH_CR_INIT;

    /* 3. Write message "abcd" = 0x61626364 (4 bytes, full 32-bit word, NBLW=0) */
    HASH_DIN = 0x61626364u;

    /* 4. Finalize: NBLW=0 (full 32-bit word valid), DCAL=1 */
    HASH_STR = 0u | HASH_STR_DCAL;

    /* 5. Wait for digest ready (BUSY=0) */
    uint32_t t;
    for (t = 0; t < TIMEOUT; t++) {
        if (!(HASH_SR & HASH_SR_BUSY)) break;
    }
    if (HASH_SR & HASH_SR_BUSY) {
        ael_mailbox_fail(0xE001u, HASH_SR);
        while (1) {}
    }

    /* 6. Read first word of SHA-256 digest */
    uint32_t digest0 = HASH_HR(0);

    /* 7. Verify */
    if (digest0 != SHA256_ABCD_W0) {
        ael_mailbox_fail(0xE002u, digest0);
        while (1) {}
    }

    AEL_MAILBOX->detail0 = digest0;   /* 0x88D4266F */
    ael_mailbox_pass();
    while (1) {}
    return 0;
}
