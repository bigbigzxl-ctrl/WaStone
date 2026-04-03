/*
 * stm32h563rgt6_pka_mailbox — PKA arithmetic addition self-test
 *
 * Uses PKA in Arithmetic Addition mode (MODE=0x09) to compute:
 *   0x00001234 + 0x00005678 = 0x000068AC
 *
 * This verifies the PKA compute engine without requiring an external coprocessor
 * or complex ECC key infrastructure.
 *
 * PKA_BASE     = AHB2 + 0xA2000 = 0x420C2000
 * PKA_RAM_BASE = AHB2 + 0xA2400 = 0x420C2400
 * RCC_AHB2ENR  = RCC_BASE + 0x08C, bit19 = PKAEN
 *
 * PKA registers:
 *   CR   +0x000: bit0=EN, bit1=START, bits[13:8]=MODE
 *   SR   +0x004: bit0=INITOK, bit16=BUSY, bit17=PROCENDF
 *   CLRFR+0x008: write to clear flags
 *
 * PKA RAM (word indices from PKA_RAM_BASE):
 *   For ARITHMETIC_ADD (MODE=0x09):
 *     NB_BITS: RAM[2]         (0x0408 - 0x0400) >> 2 = 2
 *     OP1:     RAM[404]       (0x0A50 - 0x0400) >> 2 = 404
 *     OP2:     RAM[546]       (0x0C68 - 0x0400) >> 2 = 546
 *     RESULT:  RAM[670]       (0x0E78 - 0x0400) >> 2 = 670
 *
 * Operand storage: big-endian words (MSW first), terminated with 0x00000000.
 * For 32-bit operand: write 1 word then padding 0.
 *
 * FAIL codes:
 *   0xE001 — PKA_SR.INITOK not set after enable
 *   0xE002 — PROCENDF timeout (operation did not complete)
 *   0xE003 — RAMERRF or ADDRERRF error flag set
 *   0xE004 — result mismatch, detail0 = actual result
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
#define PKA_SR_BUSY     (1u << 16)
#define PKA_SR_PROCENDF (1u << 17)
#define PKA_SR_RAMERRF  (1u << 19)
#define PKA_SR_ADDRERRF (1u << 20)
#define PKA_SR_OPERRF   (1u << 21)

#define PKA_MODE_ARITHMETIC_ADD  (0x09u)

/* RAM word-index helpers */
#define PKA_RAM_W(idx)  (*(volatile uint32_t *)(PKA_RAM_BASE + (idx) * 4u))

/* RAM word indices for ARITHMETIC_ADD (from CMSIS defines) */
#define IDX_NB_BITS  2u    /* (0x0408-0x0400)>>2 */
#define IDX_OP1      404u  /* (0x0A50-0x0400)>>2 */
#define IDX_OP2      546u  /* (0x0C68-0x0400)>>2 */
#define IDX_RESULT   670u  /* (0x0E78-0x0400)>>2 */

#define TIMEOUT  1000000u

int main(void)
{
    ael_mailbox_init();

    /* 1. Enable PKA clock (AHB2ENR bit19) */
    RCC_AHB2ENR |= (1u << 19);
    (void)RCC_AHB2ENR;

    /* 2. Enable PKA */
    PKA_CR = PKA_CR_EN;
    (void)PKA_CR;

    /* 3. Wait for INITOK */
    uint32_t t;
    for (t = 0u; t < TIMEOUT; t++) {
        if (PKA_SR & PKA_SR_INITOK) break;
    }
    if (t == TIMEOUT) {
        ael_mailbox_fail(0xE001u, PKA_SR);
        while (1) {}
    }

    /* 4. Write inputs to PKA RAM
     *    Operand storage: big-endian word order, LSW=0 terminator
     *    For 32-bit value 0x1234: write [0x00001234, 0x00000000]
     */
    PKA_RAM_W(IDX_NB_BITS) = 32u;          /* operand size in bits */
    PKA_RAM_W(IDX_OP1)     = 0x00001234u;  /* OP1 = 0x1234 */
    PKA_RAM_W(IDX_OP1 + 1) = 0u;           /* terminator */
    PKA_RAM_W(IDX_OP2)     = 0x00005678u;  /* OP2 = 0x5678 */
    PKA_RAM_W(IDX_OP2 + 1) = 0u;           /* terminator */

    /* 5. Clear previous flags */
    PKA_CLRFR = 0xFFFFFFFFu;

    /* 6. Start operation: MODE=0x09 (arithmetic add), START=1 */
    PKA_CR = PKA_CR_EN | (PKA_MODE_ARITHMETIC_ADD << 8) | PKA_CR_START;

    /* 7. Wait for PROCENDF */
    for (t = 0u; t < TIMEOUT; t++) {
        if (PKA_SR & PKA_SR_PROCENDF) break;
    }
    if (t == TIMEOUT) {
        ael_mailbox_fail(0xE002u, PKA_SR);
        while (1) {}
    }

    /* 8. Check for errors */
    uint32_t sr = PKA_SR;
    if (sr & (PKA_SR_RAMERRF | PKA_SR_ADDRERRF | PKA_SR_OPERRF)) {
        ael_mailbox_fail(0xE003u, sr);
        while (1) {}
    }

    /* 9. Read result */
    uint32_t result = PKA_RAM_W(IDX_RESULT);

    if (result != 0x000068ACu) {
        ael_mailbox_fail(0xE004u, result);
        while (1) {}
    }

    AEL_MAILBOX->detail0 = result;
    ael_mailbox_pass();
    while (1) {}
    return 0;
}
