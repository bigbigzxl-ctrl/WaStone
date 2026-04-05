/*
 * AEL Zephyr hello_loop — STM32F103 Blue Pill (stm32_min_dev)
 *
 * Console: USART1 / PA9 (TX) at 115200 8N1
 * Board:   stm32_min_dev (STM32F103xB compatible; tested on C6T6)
 *
 * Prints "AEL_ZEPHYR_IDLE count=N" every 500 ms.
 * AEL observe_uart expect_patterns: ["AEL_ZEPHYR_IDLE"]
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
