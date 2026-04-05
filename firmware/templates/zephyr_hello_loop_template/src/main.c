/*
 * AEL Zephyr hello_loop — board-agnostic smoke test firmware
 *
 * Prints "AEL_ZEPHYR_IDLE count=N" every 500 ms on the board's
 * default Zephyr console UART (defined in the board's DTS chosen node).
 *
 * Why this instead of samples/hello_world:
 *   hello_world prints once at boot. If AEL observe_uart opens the port
 *   after boot, it misses the output entirely (race condition).
 *   This firmware prints indefinitely — observe_uart can start any time.
 *
 * AEL test plan:
 *   expect_patterns: ["AEL_ZEPHYR_IDLE"]
 *   duration_s: 6   (captures ~12 prints at 500 ms cadence)
 *
 * Board-specific wiring:
 *   Connect the board's DTS console TX pin to a USB-UART adapter RXD.
 *   Console TX pin location:
 *     grep -A6 "chosen" ~/zephyrproject/zephyr/boards/<arch>/<board>/<board>.dts
 *   See docs/guides/zephyr_ael_board_onboarding.md for the full checklist.
 *
 * To use for a specific board, copy this directory to:
 *   firmware/targets/<board_id>_zephyr_hello_loop/
 * then set build.project_dir in the test plan to that path.
 * No source changes are needed — CONFIG_BOARD expands at build time.
 */

#include <zephyr/kernel.h>

int main(void)
{
    uint32_t count = 0;

    printk("AEL_ZEPHYR_READY board=" CONFIG_BOARD "\n");

    while (1) {
        printk("AEL_ZEPHYR_IDLE count=%u\n", count++);
        k_msleep(500);
    }

    return 0;
}
