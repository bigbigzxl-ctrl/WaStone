/*
 * stm32h563rgt6_sbs_mailbox — SBS (System Boot Security) register self-test
 *
 * SBS replaces SYSCFG on STM32H5. It controls hide protection levels (HDPL),
 * debug access, I/O compensation cells, and security configuration.
 *
 * Tests:
 *   1. Enable SBS APB3 clock (RCC_APB3ENR bit1 = SBSEN)
 *   2. Read HDPLSR (HDPL status) — must be accessible (non-0xFFFFFFFF)
 *   3. Read PMCR (Product Mode & Config Register) — I/O comp cell state
 *   4. Read DBGLOCKR (Debug Lock Register) — debug lock state
 *   5. Read CFGR2 (Class B register) — safety config
 *
 * SBS_BASE   = APB3PERIPH_BASE + 0x0400 = 0x44000400
 * APB3PERIPH_BASE = 0x44000000 (PERIPH_BASE + 0x04000000)
 *
 * SBS register offsets:
 *   HDPLSR   +0x14  HDPL status (bits[3:0] = current HDPL)
 *   DBGCR    +0x20  Debug control
 *   DBGLOCKR +0x24  Debug lock (0xB4 = locked, 0xC3 = unlocked)
 *   PMCR     +0x100 Product mode & config (ETH PHY sel, I/O cell)
 *   FPUIMR   +0x104 FPU interrupt mask
 *   CFGR2    +0x120 Class B register (PVDL, ECC, etc.)
 *
 * FAIL codes:
 *   0xE001 — SBS HDPLSR inaccessible (reads 0xFFFFFFFF)
 *
 * detail0: [31:16]=DBGLOCKR[7:0]<<8 | HDPLSR[7:0], [15:0]=CFGR2[15:0]
 */

#include <stdint.h>
#include "../ael_mailbox.h"

#define RCC_BASE        0x44020C00u
#define RCC_APB3ENR     (*(volatile uint32_t *)(RCC_BASE + 0x0A8u))

#define SBS_BASE        0x44000400u
#define SBS_HDPLSR      (*(volatile uint32_t *)(SBS_BASE + 0x014u))
#define SBS_DBGCR       (*(volatile uint32_t *)(SBS_BASE + 0x020u))
#define SBS_DBGLOCKR    (*(volatile uint32_t *)(SBS_BASE + 0x024u))
#define SBS_PMCR        (*(volatile uint32_t *)(SBS_BASE + 0x100u))
#define SBS_FPUIMR      (*(volatile uint32_t *)(SBS_BASE + 0x104u))
#define SBS_CFGR2       (*(volatile uint32_t *)(SBS_BASE + 0x120u))

int main(void)
{
    ael_mailbox_init();

    /* 1. Enable SBS APB3 clock */
    RCC_APB3ENR |= (1u << 1);
    (void)RCC_APB3ENR;

    /* 2. Read HDPLSR — verify accessibility */
    uint32_t hdplsr = SBS_HDPLSR;
    if (hdplsr == 0xFFFFFFFFu) {
        ael_mailbox_fail(0xE001u, hdplsr);
        while (1) {}
    }

    /* 3. Read other SBS registers */
    uint32_t dbglockr = SBS_DBGLOCKR;
    uint32_t pmcr     = SBS_PMCR;
    uint32_t cfgr2    = SBS_CFGR2;
    uint32_t fpuimr   = SBS_FPUIMR;

    (void)pmcr;
    (void)fpuimr;

    /* detail0: [31:24]=dbglockr[7:0], [23:16]=hdplsr[7:0], [15:0]=cfgr2[15:0] */
    AEL_MAILBOX->detail0 = ((dbglockr & 0xFFu) << 24)
                         | ((hdplsr   & 0xFFu) << 16)
                         | (cfgr2    & 0xFFFFu);
    ael_mailbox_pass();
    while (1) {}
    return 0;
}
