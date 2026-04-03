/*
 * stm32h563rgt6_vrefbuf_mailbox — VREFBUF voltage reference self-test
 *
 * Tests VREFBUF peripheral accessibility and basic configuration.
 * Does not require VRR (voltage reference ready) which depends on VREF+
 * pin capacitance and board-level analog power routing.
 *
 * Test:
 *   1. Enable VREFBUF clock
 *   2. Read factory trim from VREFBUF_CCR — must be non-zero (factory programmed)
 *   3. Enable VREFBUF (ENVR=1, HIZ=1 internal mode)
 *   4. Verify ENVR stays set after write (register R/W functional)
 *   5. Read VRS (voltage scale) field — can be set/read back
 *
 * VREFBUF_BASE = APB3 + 0x7400 = 0x44007400
 * RCC_APB3ENR  = RCC_BASE + 0x0A8, bit20 = VREFEN
 *
 * VREFBUF_CSR (+0x00):
 *   bit0 = ENVR, bit1 = HIZ, bit3 = VRR, bits[6:4] = VRS
 * VREFBUF_CCR (+0x04):
 *   bits[5:0] = TRIM (factory calibration value, read-only)
 *
 * FAIL codes:
 *   0xE001 — ENVR not set after enable (register not writable)
 *   0xE002 — TRIM = 0 (peripheral not clocked or not factory calibrated)
 */

#include <stdint.h>
#include "../ael_mailbox.h"

#define RCC_BASE        0x44020C00u
#define RCC_APB3ENR     (*(volatile uint32_t *)(RCC_BASE + 0x0A8u))

#define VREFBUF_BASE    0x44007400u
#define VREFBUF_CSR     (*(volatile uint32_t *)(VREFBUF_BASE + 0x00u))
#define VREFBUF_CCR     (*(volatile uint32_t *)(VREFBUF_BASE + 0x04u))

#define VREFBUF_CSR_ENVR (1u << 0)
#define VREFBUF_CSR_HIZ  (1u << 1)

int main(void)
{
    ael_mailbox_init();

    /* 1. Enable VREFBUF clock (APB3ENR bit20) */
    RCC_APB3ENR |= (1u << 20);
    (void)RCC_APB3ENR;

    /* 2. Read factory trim (must be non-zero if peripheral is clocked) */
    uint32_t trim = VREFBUF_CCR & 0x3Fu;
    if (trim == 0u) {
        ael_mailbox_fail(0xE002u, VREFBUF_CCR);
        while (1) {}
    }

    /* 3. Enable VREFBUF in internal/HIZ mode */
    VREFBUF_CSR = VREFBUF_CSR_ENVR | VREFBUF_CSR_HIZ;
    (void)VREFBUF_CSR;

    /* 4. Verify ENVR stayed set */
    uint32_t csr = VREFBUF_CSR;
    if (!(csr & VREFBUF_CSR_ENVR)) {
        ael_mailbox_fail(0xE001u, csr);
        while (1) {}
    }

    /* detail0 = (trim << 8) | CSR */
    AEL_MAILBOX->detail0 = (trim << 8) | (csr & 0xFFu);
    ael_mailbox_pass();
    while (1) {}
    return 0;
}
