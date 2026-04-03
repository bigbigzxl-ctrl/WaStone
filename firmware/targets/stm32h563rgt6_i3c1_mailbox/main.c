/*
 * stm32h563rgt6_i3c1_mailbox — I3C1 peripheral self-test
 *
 * I3C (Improved Inter-Integrated Circuit) is a new MIPI standard introduced
 * in STM32H5. It is backward-compatible with I2C but adds in-band interrupts,
 * dynamic addressing, and higher speeds (12.5 MHz).
 *
 * This test verifies I3C1 register accessibility and basic configuration:
 *   1. Enable I3C1 APB1L clock (RCC_APB1LENR bit23 = I3C1EN)
 *   2. Read I3C1 CFGR — must be accessible
 *   3. Configure I3C1 as controller (CFGR.CRINIT = bit1 = 1)
 *   4. Read DEVR0 (own target characteristics register)
 *   5. Read EPIDR (Extended Provisioned ID register)
 *   6. Read TIMINGR0/1 (timing configuration registers)
 *
 * I3C1_BASE   = APB1PERIPH_BASE + 0x5C00 = 0x40005C00
 *
 * I3C registers (offsets from I3C1_BASE):
 *   CR       +0x00  Control register
 *   CFGR     +0x04  Configuration register
 *     bit0  = EN     (enable)
 *     bit1  = CRINIT (1=controller, 0=target)
 *   SR       +0x30  Status register
 *   EVR      +0x50  Event register
 *   DEVR0    +0x60  Own target characteristics register
 *   TIMINGR0 +0xA0  Timing 0 (SCL timing)
 *   TIMINGR1 +0xA4  Timing 1
 *   BCR      +0xC0  Bus Characteristics register
 *   DCR      +0xC4  Device Characteristics register
 *   EPIDR    +0xD4  Extended Provisioned ID register
 *
 * RCC_APB1LENR = RCC_BASE + 0x0A4, bit23 = I3C1EN
 *
 * FAIL codes:
 *   0xE001 — I3C1 CFGR inaccessible (reads 0xFFFFFFFF)
 *
 * detail0: [31:16]=EPIDR[15:0], [15:8]=BCR[7:0], [7:0]=DEVR0[7:0]
 */

#include <stdint.h>
#include "../ael_mailbox.h"

#define RCC_BASE        0x44020C00u
#define RCC_APB1LENR    (*(volatile uint32_t *)(RCC_BASE + 0x0A4u))

#define I3C1_BASE       0x40005C00u
#define I3C_CFGR        (*(volatile uint32_t *)(I3C1_BASE + 0x04u))
#define I3C_SR          (*(volatile uint32_t *)(I3C1_BASE + 0x30u))
#define I3C_DEVR0       (*(volatile uint32_t *)(I3C1_BASE + 0x60u))
#define I3C_TIMINGR0    (*(volatile uint32_t *)(I3C1_BASE + 0xA0u))
#define I3C_TIMINGR1    (*(volatile uint32_t *)(I3C1_BASE + 0xA4u))
#define I3C_BCR         (*(volatile uint32_t *)(I3C1_BASE + 0xC0u))
#define I3C_DCR         (*(volatile uint32_t *)(I3C1_BASE + 0xC4u))
#define I3C_EPIDR       (*(volatile uint32_t *)(I3C1_BASE + 0xD4u))

#define I3C_CFGR_CRINIT (1u << 1)   /* Controller mode init */

int main(void)
{
    ael_mailbox_init();

    /* 1. Enable I3C1 APB1L clock (bit23) */
    RCC_APB1LENR |= (1u << 23);
    (void)RCC_APB1LENR;

    /* 2. Verify accessibility */
    uint32_t cfgr = I3C_CFGR;
    if (cfgr == 0xFFFFFFFFu) {
        ael_mailbox_fail(0xE001u, cfgr);
        while (1) {}
    }

    /* 3. Set CRINIT (select controller mode — must be done before EN) */
    I3C_CFGR = I3C_CFGR_CRINIT;
    (void)I3C_CFGR;

    /* 4. Read device characteristic registers */
    uint32_t devr0   = I3C_DEVR0;
    uint32_t bcr     = I3C_BCR;
    uint32_t dcr     = I3C_DCR;
    uint32_t epidr   = I3C_EPIDR;
    uint32_t timing0 = I3C_TIMINGR0;
    uint32_t sr      = I3C_SR;

    (void)dcr;
    (void)timing0;
    (void)sr;

    /* detail0: [31:16]=EPIDR[15:0], [15:8]=BCR[7:0], [7:0]=DEVR0[7:0] */
    AEL_MAILBOX->detail0 = ((epidr & 0xFFFFu) << 16)
                         | ((bcr   & 0xFFu)   << 8)
                         | (devr0  & 0xFFu);
    ael_mailbox_pass();
    while (1) {}
    return 0;
}
