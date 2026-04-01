/*
 * STM32F407 — Hardware CRC Unit Test
 *
 * Verifies the CRC peripheral against a pure-C software implementation
 * of the same algorithm (CRC-32/MPEG-2).
 *
 * Algorithm: CRC-32/MPEG-2
 *   Polynomial : 0x04C11DB7
 *   Init       : 0xFFFFFFFF
 *   RefIn/Out  : false  (MSB first, no bit reflection)
 *   XorOut     : 0x00000000
 *
 * Test data: 8 words of known pattern.
 * PASS: hw_result == sw_result (cross-verification).
 * FAIL: ERR_STUCK(1)   — CRC_DR never changes from reset value
 *       ERR_MISMATCH(2) — hw != sw
 *       ERR_REPRO(3)    — second run gives different hw result
 *
 * No external wiring required. No PLL — 16 MHz HSI only.
 * Mailbox: 0x2001FC00
 */

#include <stdint.h>

#define AEL_MAILBOX_ADDR 0x2001FC00u
#include "ael_mailbox.h"

void HardFault_Handler(void)
{
    *((volatile uint32_t *)0xE000ED0Cu) = 0x05FA0004u;
    while (1) {}
}

/* ---- RCC ---------------------------------------------------------------- */
#define RCC_AHB1ENR  (*(volatile uint32_t *)(0x40023800u + 0x30u))
#define RCC_AHB1ENR_CRCEN (1u << 12)

/* ---- CRC unit (base 0x40023000) ----------------------------------------- */
#define CRC_DR  (*(volatile uint32_t *)0x40023000u)
#define CRC_IDR (*(volatile uint32_t *)0x40023004u)
#define CRC_CR  (*(volatile uint32_t *)0x40023008u)
#define CRC_CR_RESET (1u << 0)

/* Error codes */
#define ERR_STUCK    1u
#define ERR_MISMATCH 2u
#define ERR_REPRO    3u

/* Test data — 8 words */
static const uint32_t data[8] = {
    0x12345678u, 0xDEADBEEFu, 0xCAFEBABEu, 0xA5A5A5A5u,
    0x00FF00FFu, 0xFFFFFFFFu, 0x01234567u, 0x89ABCDEFu,
};
#define N_DATA 8u

/* Software CRC-32/MPEG-2: same algorithm as STM32 HW CRC unit.
 * Processes word MSB-first (bit 31 → bit 0). */
static uint32_t sw_crc32_word(uint32_t crc, uint32_t word)
{
    for (int i = 31; i >= 0; i--) {
        uint32_t bit = (word >> (uint32_t)i) & 1u;
        if (((crc >> 31u) & 1u) ^ bit) {
            crc = (crc << 1u) ^ 0x04C11DB7u;
        } else {
            crc = crc << 1u;
        }
    }
    return crc;
}

static uint32_t hw_crc_run(void)
{
    CRC_CR = CRC_CR_RESET;          /* reset CRC unit → DR = 0xFFFFFFFF */
    (void)CRC_CR;
    for (uint32_t i = 0u; i < N_DATA; i++) {
        CRC_DR = data[i];
    }
    return CRC_DR;                  /* read result */
}

int main(void)
{
    /* Enable CRC clock (AHB1, bit 12) */
    RCC_AHB1ENR |= RCC_AHB1ENR_CRCEN;
    (void)RCC_AHB1ENR;

    ael_mailbox_init();

    /* ---- Software CRC ---- */
    uint32_t sw = 0xFFFFFFFFu;
    for (uint32_t i = 0u; i < N_DATA; i++) {
        sw = sw_crc32_word(sw, data[i]);
    }

    /* ---- Hardware CRC (first run) ---- */
    uint32_t hw1 = hw_crc_run();

    /* Sanity: hw must not be stuck at reset value */
    if (hw1 == 0xFFFFFFFFu) {
        ael_mailbox_fail(ERR_STUCK, hw1);
        while (1) {}
    }

    /* Cross-verify: HW must match SW */
    if (hw1 != sw) {
        ael_mailbox_fail(ERR_MISMATCH, hw1);
        while (1) {}
    }

    /* Reproducibility: second HW run must give same result */
    uint32_t hw2 = hw_crc_run();
    if (hw2 != hw1) {
        ael_mailbox_fail(ERR_REPRO, hw2);
        while (1) {}
    }

    ael_mailbox_pass();
    AEL_MAILBOX->detail0 = hw1;   /* report CRC value */

    while (1) {}
}
