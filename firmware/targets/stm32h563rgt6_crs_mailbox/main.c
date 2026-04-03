/*
 * stm32h563rgt6_crs_mailbox — CRS (Clock Recovery System) + HSI48 self-test
 *
 * Tests CRS peripheral and HSI48 oscillator:
 *   1. Enable HSI48 (48 MHz RC oscillator for USB/CRS)
 *   2. Wait for HSI48RDY
 *   3. Enable CRS clock, verify TRIM field is non-zero (factory calibrated)
 *   4. Read HSI48CAL from RCC_CRRCR (10-bit factory calibration)
 *   5. Enable CRS counter (CEN=1), verify ISR is readable
 *
 * CRS is unique to H5 (and a few other STM32 families) — it trims HSI48
 * to a reference clock (USB SOF, LSE, or GPIO) to maintain ±500 ppm accuracy.
 * This test verifies register accessibility and factory calibration state.
 *
 * CRS_BASE  = APB1 + 0x6000 = 0x40006000
 * RCC_BASE  = 0x44020C00
 * RCC_APB1LENR bit24 = CRSEN
 * RCC_CR    bit12 = HSI48ON, bit13 = HSI48RDY
 * RCC_CRRCR (+0x14): bits[9:0] = HSI48CAL (10-bit factory cal)
 *
 * CRS_CR: bit5=CEN, bits[13:8]=TRIM (6-bit)
 * CRS_ISR: bit0=SYNCOKF, bit1=SYNCWARNF, bit2=ERRF, bit3=ESYNCF
 *
 * FAIL codes:
 *   0xE001 — HSI48 not ready after timeout
 *   0xE002 — CRS_CR.TRIM = 0 (peripheral not clocked or not factory calibrated)
 *   0xE003 — HSI48CAL = 0 (RCC_CRRCR not accessible / HSI48 not running)
 */

#include <stdint.h>
#include "../ael_mailbox.h"

#define RCC_BASE        0x44020C00u
#define RCC_CR          (*(volatile uint32_t *)(RCC_BASE + 0x000u))
#define RCC_CRRCR       (*(volatile uint32_t *)(RCC_BASE + 0x014u))
#define RCC_APB1LENR    (*(volatile uint32_t *)(RCC_BASE + 0x0A8u))

#define RCC_CR_HSI48ON   (1u << 12)
#define RCC_CR_HSI48RDY  (1u << 13)
#define RCC_APB1LENR_CRSEN (1u << 24)

#define CRS_BASE        0x40006000u
#define CRS_CR          (*(volatile uint32_t *)(CRS_BASE + 0x00u))
#define CRS_CFGR        (*(volatile uint32_t *)(CRS_BASE + 0x04u))
#define CRS_ISR         (*(volatile uint32_t *)(CRS_BASE + 0x08u))

#define CRS_CR_CEN      (1u << 5)
#define CRS_CR_TRIM_Pos 8u
#define CRS_CR_TRIM_Msk (0x3Fu << CRS_CR_TRIM_Pos)

#define TIMEOUT 1000000u

int main(void)
{
    ael_mailbox_init();

    /* 1. Enable HSI48 oscillator */
    RCC_CR |= RCC_CR_HSI48ON;
    uint32_t t;
    for (t = 0u; t < TIMEOUT; t++) {
        if (RCC_CR & RCC_CR_HSI48RDY) break;
    }
    if (t == TIMEOUT) {
        ael_mailbox_fail(0xE001u, RCC_CR);
        while (1) {}
    }

    /* 2. Enable CRS peripheral clock */
    RCC_APB1LENR |= RCC_APB1LENR_CRSEN;
    (void)RCC_APB1LENR;

    /* 3. Read factory trim value from CRS_CR.TRIM[13:8] */
    uint32_t trim = (CRS_CR & CRS_CR_TRIM_Msk) >> CRS_CR_TRIM_Pos;
    if (trim == 0u) {
        ael_mailbox_fail(0xE002u, CRS_CR);
        while (1) {}
    }

    /* 4. Read HSI48 factory calibration from RCC_CRRCR bits[9:0] */
    uint32_t hsi48cal = RCC_CRRCR & 0x3FFu;
    if (hsi48cal == 0u) {
        ael_mailbox_fail(0xE003u, RCC_CRRCR);
        while (1) {}
    }

    /* 5. Enable CRS counter and read ISR */
    CRS_CR |= CRS_CR_CEN;
    (void)CRS_CR;
    uint32_t isr = CRS_ISR;

    /* detail0: [31:24]=ISR, [23:16]=trim, [15:0]=hsi48cal */
    AEL_MAILBOX->detail0 = ((isr & 0xFFu) << 24) | (trim << 16) | (hsi48cal & 0x3FFu);
    ael_mailbox_pass();
    while (1) {}
    return 0;
}
