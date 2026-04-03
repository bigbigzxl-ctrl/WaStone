/*
 * stm32h563rgt6_pka_mailbox — PKA INITOK diagnostic
 *
 * Test HAL-exact sequence: EN=1, wait INITOK bit0 FIRST, then CLRFR.
 * Previous attempts did CLRFR before INITOK wait — CLRFR writes OPERRFC
 * which may reset INITOK per RM, preventing bit0 from ever firing.
 *
 * FAIL codes:
 *   0xE001a — INITOK bit0 timeout (detail0=SR at timeout)
 *   0xE001b — INITOK bit0 never set, only bit1 (detail0=SR)
 *   0xE002  — PROCENDF timeout
 *   0xE003  — error flag set
 *   0xE004  — result mismatch
 *   0xE00F  — EN-stick timeout
 */

#include <stdint.h>
#include "../ael_mailbox.h"

#define RCC_BASE_S      0x54020C00u
#define RCC_AHB2ENR     (*(volatile uint32_t *)(RCC_BASE_S + 0x08Cu))
#define RCC_AHB2RSTR    (*(volatile uint32_t *)(RCC_BASE_S + 0x064u))

#define PKA_BASE        0x520C2000u
#define PKA_RAM_BASE    0x520C2400u
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
#define IDX_OP2      538u
#define IDX_RESULT   670u

#define TIMEOUT         1000000u
#define INITOK_TIMEOUT  20000000u   /* 5 seconds at 4 MHz CSI */

int main(void)
{
    ael_mailbox_init();

    RCC_AHB2ENR |= (1u << 19);
    (void)RCC_AHB2ENR;

    RCC_AHB2RSTR |= (1u << 19);
    (void)RCC_AHB2RSTR;
    RCC_AHB2RSTR &= ~(1u << 19);
    (void)RCC_AHB2RSTR;

    /* Enable PKA (EN-stick loop) */
    uint32_t t;
    for (t = 0u; t < TIMEOUT; t++) {
        PKA_CR = PKA_CR_EN;
        if (PKA_CR & PKA_CR_EN) break;
    }
    if (t == TIMEOUT) {
        ael_mailbox_fail(0xE00Fu, PKA_CR);
        while (1) {}
    }

    /* HAL-exact: wait for INITOK bit0 BEFORE any CLRFR.
     * Do NOT clear flags here — CLRFR.OPERRFC may reset INITOK per RM. */
    for (t = 0u; t < INITOK_TIMEOUT; t++) {
        if (PKA_SR & PKA_SR_INITOK) break;   /* bit0 only, HAL-exact */
    }
    if (t == INITOK_TIMEOUT) {
        /* Report what SR looks like at timeout */
        ael_mailbox_fail(0xE001u, PKA_SR);
        while (1) {}
    }

    /* INITOK bit0 fired — now clear flags (HAL sequence) */
    PKA_CLRFR = (1u << 21) | (1u << 20) | (1u << 19) | (1u << 17);
    (void)PKA_CLRFR;

    /* Write operands (NB_BITS=32, 1-word each + 2 terminators) */
    PKA_RAM_W(IDX_NB_BITS) = 32u;
    PKA_RAM_W(IDX_OP1 + 0) = 0x00001234u;
    PKA_RAM_W(IDX_OP1 + 1) = 0u;
    PKA_RAM_W(IDX_OP1 + 2) = 0u;
    PKA_RAM_W(IDX_OP2 + 0) = 0x00005678u;
    PKA_RAM_W(IDX_OP2 + 1) = 0u;
    PKA_RAM_W(IDX_OP2 + 2) = 0u;

    /* Set MODE via read-modify-write, then START */
    {
        uint32_t cr = PKA_CR;
        cr = (cr & ~(0x3Fu << 8)) | (PKA_MODE_ARITHMETIC_ADD << 8);
        PKA_CR = cr;
        (void)PKA_CR;
        PKA_CR = cr | PKA_CR_START;
    }

    /* Wait for PROCENDF or any error */
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

    uint32_t result = PKA_RAM_W(IDX_RESULT + 0u);
    if (result != 0x000068ACu) {
        ael_mailbox_fail(0xE004u, result);
        while (1) {}
    }

    AEL_MAILBOX->detail0 = result;
    ael_mailbox_pass();
    while (1) {}
    return 0;
}
