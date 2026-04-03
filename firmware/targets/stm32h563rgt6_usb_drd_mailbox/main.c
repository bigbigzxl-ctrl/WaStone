/*
 * stm32h563rgt6_usb_drd_mailbox — USB DRD peripheral register self-test
 *
 * Verifies that the USB FS DRD (Dual Role Device) peripheral is accessible
 * and responds to basic register reads/writes. No USB enumeration is performed.
 *
 * USB_DRD on H563:
 *   - Full-speed (12 Mbit/s) USB 2.0
 *   - Dual-role: Device or Host
 *   - Internal 48 MHz clock from HSI48 (enabled via CRS)
 *   - Clock source: requires HSI48 to be running
 *
 * Steps:
 *   1. Enable HSI48 oscillator (required for USB clock)
 *   2. Enable USB APB2 clock (RCC_APB2ENR bit24 = USBEN)
 *   3. Read USB CNTR (Control Register) — after reset = 0x0003 (PDWN=1, FRES=1)
 *   4. Clear power-down: CNTR.PDWN = 0, leave FRES = 1
 *   5. Read CNTR again to verify PDWN cleared
 *   6. Read ISTR (Interrupt Status Register)
 *   7. Read FNR (Frame Number Register)
 *   8. Read BCDR (Battery Charging Detection Register)
 *   9. Restore PDWN=1 (power down, restore state)
 *
 * USB_DRD_BASE = APB2PERIPH_BASE + 0x6000 = 0x40016000
 *
 * USB DRD register offsets:
 *   CNTR   +0x40  Control register (bit0=FRES, bit1=PDWN)
 *   ISTR   +0x44  Interrupt status
 *   FNR    +0x48  Frame number register
 *   DADDR  +0x4C  Device address register
 *   LPMCSR +0x54  LPM control/status
 *   BCDR   +0x58  Battery charging detection
 *
 * RCC:
 *   RCC_CR       bit12 = HSI48ON, bit13 = HSI48RDY
 *   RCC_APB2ENR  = RCC_BASE + 0x0AC, bit24 = USBEN
 *
 * FAIL codes:
 *   0xE001 — HSI48 not ready
 *   0xE002 — USB CNTR reads 0xFFFFFFFF (clock not enabled)
 *   0xE003 — PDWN did not clear (CNTR bit1 still set after write)
 *
 * detail0: [31:16]=BCDR[15:0], [15:8]=FNR[7:0], [7:0]=ISTR[7:0]
 */

#include <stdint.h>
#include "../ael_mailbox.h"

#define RCC_BASE        0x44020C00u
#define RCC_CR          (*(volatile uint32_t *)(RCC_BASE + 0x000u))
#define RCC_APB2ENR     (*(volatile uint32_t *)(RCC_BASE + 0x0ACu))

#define RCC_CR_HSI48ON   (1u << 12)
#define RCC_CR_HSI48RDY  (1u << 13)

#define USB_DRD_BASE    0x40016000u
#define USB_CNTR        (*(volatile uint32_t *)(USB_DRD_BASE + 0x40u))
#define USB_ISTR        (*(volatile uint32_t *)(USB_DRD_BASE + 0x44u))
#define USB_FNR         (*(volatile uint32_t *)(USB_DRD_BASE + 0x48u))
#define USB_DADDR       (*(volatile uint32_t *)(USB_DRD_BASE + 0x4Cu))
#define USB_BCDR        (*(volatile uint32_t *)(USB_DRD_BASE + 0x58u))

#define USB_CNTR_FRES   (1u << 0)
#define USB_CNTR_PDWN   (1u << 1)

#define TIMEOUT 1000000u

int main(void)
{
    ael_mailbox_init();

    /* 1. Enable HSI48 (USB clock source) */
    RCC_CR |= RCC_CR_HSI48ON;
    uint32_t t;
    for (t = 0u; t < TIMEOUT; t++) {
        if (RCC_CR & RCC_CR_HSI48RDY) break;
    }
    if (t == TIMEOUT) {
        ael_mailbox_fail(0xE001u, RCC_CR);
        while (1) {}
    }

    /* 2. Enable USB APB2 clock */
    RCC_APB2ENR |= (1u << 24);
    (void)RCC_APB2ENR;

    /* 3. Read CNTR — expect PDWN=1, FRES=1 after reset */
    uint32_t cntr = USB_CNTR;
    if (cntr == 0xFFFFFFFFu) {
        ael_mailbox_fail(0xE002u, cntr);
        while (1) {}
    }

    /* 4. Clear PDWN (power-up the USB transceiver), keep FRES */
    USB_CNTR = USB_CNTR_FRES;   /* PDWN=0, FRES=1 */
    (void)USB_CNTR;

    /* 5. Verify PDWN cleared */
    cntr = USB_CNTR;
    if (cntr & USB_CNTR_PDWN) {
        ael_mailbox_fail(0xE003u, cntr);
        while (1) {}
    }

    /* 6-8. Read status registers */
    uint32_t istr = USB_ISTR;
    uint32_t fnr  = USB_FNR;
    uint32_t bcdr = USB_BCDR;

    /* 9. Restore: power-down USB */
    USB_CNTR = USB_CNTR_PDWN | USB_CNTR_FRES;

    /* detail0: [31:16]=BCDR[15:0], [15:8]=FNR[7:0], [7:0]=ISTR[7:0] */
    AEL_MAILBOX->detail0 = ((bcdr & 0xFFFFu) << 16)
                         | ((fnr  & 0xFFu)   << 8)
                         | (istr  & 0xFFu);
    ael_mailbox_pass();
    while (1) {}
    return 0;
}
