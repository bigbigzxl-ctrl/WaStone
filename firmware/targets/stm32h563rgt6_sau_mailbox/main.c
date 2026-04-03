/*
 * stm32h563rgt6_sau_mailbox — Cortex-M33 SAU (Security Attribution Unit) self-test
 *
 * The SAU is a Cortex-M33 core feature (TrustZone). It partitions the address
 * space into Secure and Non-Secure regions. It is accessible at fixed CPU
 * addresses regardless of any peripheral clock.
 *
 * This test reads:
 *   SAU_CTRL  — enable/disable, ALLNS (all non-secure mode)
 *   SAU_TYPE  — number of SAU regions implemented (bits[7:0])
 *   SAU_RNR   — region number register
 *   SAU_RBAR  — region base address (for region 0 if any exist)
 *   SAU_RLAR  — region limit address + attributes
 *
 * Cortex-M33 SAU addresses (fixed, architecture-defined):
 *   SAU_CTRL  = 0xE000EDD0
 *   SAU_TYPE  = 0xE000EDD4
 *   SAU_RNR   = 0xE000EDD8
 *   SAU_RBAR  = 0xE000EDDC
 *   SAU_RLAR  = 0xE000EDE0
 *
 * SAU_TYPE bits[7:0] = SREGION = number of SAU regions.
 * STM32H563 with TrustZone enabled typically has 8 SAU regions.
 *
 * SAU_CTRL bits:
 *   bit0 = ENABLE (SAU enabled)
 *   bit1 = ALLNS  (1 = all memory is Non-Secure, SAU disabled effectively)
 *
 * FAIL codes:
 *   0xE001 — SAU_TYPE reads 0xFFFFFFFF (bus fault / register not accessible)
 *
 * Note: SREGION=0 is valid when TZEN=0 (SAU registers are Secure-only → RAZ from NS).
 *       This test passes in both TZEN=0 (RAZ) and TZEN=1 (real regions) cases.
 *
 * detail0: [31:16]=SAU_CTRL[15:0], [15:0]=SAU_TYPE[7:0]
 */

#include <stdint.h>
#include "../ael_mailbox.h"

/* Cortex-M33 SAU — fixed architecture addresses, no clock needed */
#define SAU_CTRL    (*(volatile uint32_t *)0xE000EDD0u)
#define SAU_TYPE    (*(volatile uint32_t *)0xE000EDD4u)
#define SAU_RNR     (*(volatile uint32_t *)0xE000EDD8u)
#define SAU_RBAR    (*(volatile uint32_t *)0xE000EDDCu)
#define SAU_RLAR    (*(volatile uint32_t *)0xE000EDE0u)

int main(void)
{
    ael_mailbox_init();

    /* Read SAU_TYPE — always accessible on Cortex-M33 */
    uint32_t sau_type = SAU_TYPE;
    if (sau_type == 0xFFFFFFFFu) {
        ael_mailbox_fail(0xE001u, sau_type);
        while (1) {}
    }

    uint32_t sregion = sau_type & 0xFFu;
    uint32_t sau_ctrl = SAU_CTRL;

    /* SREGION=0 is valid (TZEN=0 → SAU Secure-only → RAZ from NS mode).
     * Only fail if SAU_TYPE == 0xFFFFFFFF (bus not responding). */

    /* If SAU has regions, read region 0 info */
    uint32_t rbar = 0u, rlar = 0u;
    if (sregion > 0u) {
        SAU_RNR = 0u;          /* select region 0 */
        (void)SAU_RNR;
        rbar = SAU_RBAR;
        rlar = SAU_RLAR;
    }
    (void)rbar;
    (void)rlar;

    /* detail0: [31:16]=SAU_CTRL, [15:0]=SREGION count */
    AEL_MAILBOX->detail0 = ((sau_ctrl & 0xFFFFu) << 16) | (sregion & 0xFFFFu);
    ael_mailbox_pass();
    while (1) {}
    return 0;
}
