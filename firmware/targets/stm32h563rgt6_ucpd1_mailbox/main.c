/*
 * stm32h563rgt6_ucpd1_mailbox — UCPD1 (USB Type-C Power Delivery) self-test
 *
 * UCPD (USB-C Power Delivery) manages the CC lines for USB Type-C
 * connection detection, role negotiation, and power contract negotiation.
 * This test verifies register accessibility without initiating any
 * PD message exchange.
 *
 * Steps:
 *   1. Enable UCPD1 APB1H clock (RCC_APB1HENR bit23 = UCPD1EN)
 *   2. Read UCPD1 CFG1 register — verify accessible
 *   3. Read CFG2 and CFG3
 *   4. Configure CFG1 with a known PSCx/TRANSWIN value (no enable yet)
 *   5. Read back CFG1 to verify write took effect
 *   6. Read SR (status) — should be 0 (no activity)
 *
 * UCPD1_BASE = APB1PERIPH_BASE + 0xDC00 = 0x4000DC00
 * RCC_APB1HENR = RCC_BASE + 0x0A0, bit23 = UCPD1EN
 *
 * UCPD registers:
 *   CFG1  +0x00: PSCx[2:0]=bits[19:17], TRANSWIN[4:0]=bits[16:12],
 *                IFRGAP[4:0]=bits[11:7], HBITCLKDIV[5:0]=bits[5:0]
 *   CFG2  +0x04: bit0=RXFILTDIS, bit1=RXFILTSCA, bit2=CC1TRIS, bit3=CC2TRIS
 *   CFG3  +0x08: bit0=BUSYLK
 *   CR    +0x0C: bit0=USBPDDRST
 *   SR    +0x14: status flags
 *
 * FAIL codes:
 *   0xE001 — UCPD1 CFG1 inaccessible (reads 0xFFFFFFFF)
 *   0xE002 — CFG1 write not retained (readback mismatch)
 *
 * detail0: [31:16]=CFG2[15:0], [15:0]=CFG1[15:0]
 */

#include <stdint.h>
#include "../ael_mailbox.h"

#define RCC_BASE        0x44020C00u
#define RCC_APB1HENR    (*(volatile uint32_t *)(RCC_BASE + 0x0A0u))

#define UCPD1_BASE      0x4000DC00u
#define UCPD_CFG1       (*(volatile uint32_t *)(UCPD1_BASE + 0x00u))
#define UCPD_CFG2       (*(volatile uint32_t *)(UCPD1_BASE + 0x04u))
#define UCPD_CFG3       (*(volatile uint32_t *)(UCPD1_BASE + 0x08u))
#define UCPD_CR         (*(volatile uint32_t *)(UCPD1_BASE + 0x0Cu))
#define UCPD_SR         (*(volatile uint32_t *)(UCPD1_BASE + 0x14u))

/*
 * CFG1 test value: PSC=1 (div2), TRANSWIN=5, IFRGAP=17, HBITCLKDIV=13
 * These are typical 300 kHz BMC clock values for USB PD at 48 MHz
 * PSC[19:17]=001, TRANSWIN[16:12]=00101, IFRGAP[11:7]=10001, HBITCLK[5:0]=001101
 */
#define CFG1_TEST  ((1u<<17) | (5u<<12) | (17u<<7) | 13u)

int main(void)
{
    ael_mailbox_init();

    /* 1. Enable UCPD1 clock (APB1HENR bit23) */
    RCC_APB1HENR |= (1u << 23);
    (void)RCC_APB1HENR;

    /* 2. Verify accessibility */
    uint32_t cfg1 = UCPD_CFG1;
    if (cfg1 == 0xFFFFFFFFu) {
        ael_mailbox_fail(0xE001u, cfg1);
        while (1) {}
    }

    /* 3. Write CFG1 with known timing values (UCPD not enabled yet) */
    UCPD_CFG1 = CFG1_TEST;
    (void)UCPD_CFG1;

    /* 4. Read back CFG1 */
    cfg1 = UCPD_CFG1;
    if ((cfg1 & 0x000FFFFFu) != (CFG1_TEST & 0x000FFFFFu)) {
        ael_mailbox_fail(0xE002u, cfg1);
        while (1) {}
    }

    /* 5. Read remaining registers */
    uint32_t cfg2 = UCPD_CFG2;
    uint32_t sr   = UCPD_SR;
    (void)sr;

    /* detail0: [31:16]=cfg2[15:0], [15:0]=cfg1[15:0] */
    AEL_MAILBOX->detail0 = ((cfg2 & 0xFFFFu) << 16) | (cfg1 & 0xFFFFu);
    ael_mailbox_pass();
    while (1) {}
    return 0;
}
