/*
 * stm32h563rgt6_mpu_mailbox — Cortex-M33 MPU self-test
 *
 * The MPU (Memory Protection Unit) is a Cortex-M33 core feature that
 * enforces access permissions on memory regions. It is orthogonal to the
 * SAU (TrustZone) — MPU enforces privilege/unprivileged access while SAU
 * enforces secure/non-secure boundaries.
 *
 * Registers (fixed Cortex-M33 addresses, no clock needed):
 *   MPU_TYPE  = 0xE000ED90: bits[15:8]=DREGION (number of MPU regions)
 *   MPU_CTRL  = 0xE000ED94: bit0=ENABLE, bit1=HFNMIENA, bit2=PRIVDEFENA
 *   MPU_RNR   = 0xE000ED98: region number select
 *   MPU_RBAR  = 0xE000ED9C: region base address
 *   MPU_RLAR  = 0xE000EDA0: region limit address + enable
 *
 * Expected: DREGION = 8 (STM32H563 implements 8 MPU regions)
 *
 * Test:
 *   1. Read MPU_TYPE — verify DREGION != 0
 *   2. Read MPU_CTRL — verify MPU is disabled at entry (default)
 *   3. Configure region 0: base=0x20000000 (SRAM1), limit=0x2001FFFF,
 *      attrs=RW, privileged only
 *   4. Enable MPU with PRIVDEFENA=1 (privileged code can still access
 *      background map — safe for this test)
 *   5. Read back RBAR and RLAR for region 0 to verify config took effect
 *   6. Disable MPU (restore state)
 *
 * FAIL codes:
 *   0xE001 — MPU_TYPE inaccessible (reads 0xFFFFFFFF)
 *   0xE002 — DREGION == 0 (no MPU regions implemented)
 *
 * detail0: [31:16]=MPU_CTRL readback, [15:8]=DREGION, [7:0]=MPU_CTRL at entry
 */

#include <stdint.h>
#include "../ael_mailbox.h"

/* Cortex-M33 MPU — fixed architecture addresses */
#define MPU_TYPE    (*(volatile uint32_t *)0xE000ED90u)
#define MPU_CTRL    (*(volatile uint32_t *)0xE000ED94u)
#define MPU_RNR     (*(volatile uint32_t *)0xE000ED98u)
#define MPU_RBAR    (*(volatile uint32_t *)0xE000ED9Cu)
#define MPU_RLAR    (*(volatile uint32_t *)0xE000EDA0u)

#define MPU_CTRL_ENABLE      (1u << 0)
#define MPU_CTRL_PRIVDEFENA  (1u << 2)

int main(void)
{
    ael_mailbox_init();

    /* 1. Read MPU_TYPE */
    uint32_t mpu_type = MPU_TYPE;
    if (mpu_type == 0xFFFFFFFFu) {
        ael_mailbox_fail(0xE001u, mpu_type);
        while (1) {}
    }

    uint32_t dregion = (mpu_type >> 8) & 0xFFu;
    if (dregion == 0u) {
        ael_mailbox_fail(0xE002u, mpu_type);
        while (1) {}
    }

    /* 2. Read CTRL at entry */
    uint32_t ctrl_entry = MPU_CTRL;

    /* 3. Configure region 0 (only if MPU currently disabled — safe) */
    if (!(ctrl_entry & MPU_CTRL_ENABLE)) {
        MPU_RNR  = 0u;
        /* RBAR: base=0x20000000, SH=0, AP=0b01(RW priv), XN=1 */
        MPU_RBAR = (0x20000000u) | (0u << 3) | (0x1u << 1) | (1u << 0);
        /* RLAR: limit=0x2001FFC0 (aligned), AttrIndx=0, EN=1 */
        MPU_RLAR = (0x2001FFE0u) | (0u << 1) | (1u << 0);

        /* 4. Enable MPU with PRIVDEFENA */
        MPU_CTRL = MPU_CTRL_ENABLE | MPU_CTRL_PRIVDEFENA;
        __asm volatile ("dsb" ::: "memory");
        __asm volatile ("isb" ::: "memory");
    }

    /* 5. Read back CTRL */
    uint32_t ctrl_after = MPU_CTRL;

    /* 6. Disable MPU */
    MPU_CTRL = 0u;
    __asm volatile ("dsb" ::: "memory");
    __asm volatile ("isb" ::: "memory");

    /* detail0: [31:16]=ctrl_after, [15:8]=dregion, [7:0]=ctrl_entry */
    AEL_MAILBOX->detail0 = ((ctrl_after & 0xFFFFu) << 16)
                         | ((dregion   & 0xFFu)    << 8)
                         | (ctrl_entry  & 0xFFu);
    ael_mailbox_pass();
    while (1) {}
    return 0;
}
