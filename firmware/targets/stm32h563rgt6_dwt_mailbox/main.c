/*
 * stm32h563rgt6_dwt_mailbox — Cortex-M33 DWT cycle counter self-test
 *
 * DWT (Data Watchpoint and Trace) is a Cortex-M33 debug/trace subsystem.
 * Its CYCCNT register counts CPU clock cycles. When enabled via CoreDebug
 * DEMCR.TRCENA, the cycle counter free-runs at CPU clock frequency.
 *
 * This test verifies CYCCNT advances, which confirms:
 *   - DWT is accessible and functional
 *   - CoreDebug trace infrastructure works
 *   - CPU clock is running (64 MHz HSI default)
 *
 * Cortex-M33 fixed addresses (no peripheral clock needed):
 *   CoreDebug_DEMCR = 0xE000EDFC  bit24=TRCENA (enables DWT)
 *   DWT_CTRL        = 0xE0001000  bit0=CYCCNTENA (enables CYCCNT)
 *   DWT_CYCCNT      = 0xE0001004  CPU cycle counter
 *   DWT_CPICNT      = 0xE0001008  CPI count
 *
 * FAIL codes:
 *   0xE001 — DWT_CTRL reads 0xFFFFFFFF (DWT not accessible)
 *   0xE002 — CYCCNT did not advance (counter stuck)
 *
 * detail0: delta between two CYCCNT reads (number of cycles elapsed)
 */

#include <stdint.h>
#include "../ael_mailbox.h"

/* Cortex-M33 CoreDebug and DWT — fixed architecture addresses */
#define CoreDebug_DEMCR  (*(volatile uint32_t *)0xE000EDFCu)
#define DWT_CTRL         (*(volatile uint32_t *)0xE0001000u)
#define DWT_CYCCNT       (*(volatile uint32_t *)0xE0001004u)

#define DEMCR_TRCENA     (1u << 24)
#define DWT_CTRL_CYCCNTENA (1u << 0)

int main(void)
{
    ael_mailbox_init();

    /* 1. Enable trace subsystem (TRCENA must be set before accessing DWT) */
    CoreDebug_DEMCR |= DEMCR_TRCENA;
    __asm volatile ("dsb" ::: "memory");

    /* 2. Verify DWT accessible */
    uint32_t ctrl = DWT_CTRL;
    if (ctrl == 0xFFFFFFFFu) {
        ael_mailbox_fail(0xE001u, ctrl);
        while (1) {}
    }

    /* 3. Reset and enable CYCCNT */
    DWT_CYCCNT = 0u;
    DWT_CTRL  |= DWT_CTRL_CYCCNTENA;

    /* 4. Read CYCCNT twice with a spin delay between */
    uint32_t c1 = DWT_CYCCNT;
    for (volatile uint32_t d = 0u; d < 1000u; d++) {}
    uint32_t c2 = DWT_CYCCNT;

    /* 5. Verify counter advanced */
    if (c1 == c2) {
        ael_mailbox_fail(0xE002u, c1);
        while (1) {}
    }

    /* 6. Disable CYCCNT and TRCENA (restore state) */
    DWT_CTRL        &= ~DWT_CTRL_CYCCNTENA;
    CoreDebug_DEMCR &= ~DEMCR_TRCENA;

    /* detail0: cycle delta (c2 - c1) = cycles in spin loop */
    AEL_MAILBOX->detail0 = c2 - c1;
    ael_mailbox_pass();
    while (1) {}
    return 0;
}
