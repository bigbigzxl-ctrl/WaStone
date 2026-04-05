/*
 * AEL Zephyr hello_loop — STM32F4 Discovery
 *
 * Prints "AEL_ZEPHYR_IDLE count=N" every 500 ms on USART2/PA2.
 * Designed to be observable by AEL observe_uart without a race
 * condition: the pattern repeats indefinitely, so observe_uart
 * can start any time after flash and still catch it.
 *
 * Verified on: stm32f4_disco / STM32F407VGT6 / Zephyr 4.4.0-rc2
 */

#include <zephyr/kernel.h>

int main(void)
{
    uint32_t count = 0;

    printk("AEL_ZEPHYR_READY board=stm32f4_disco\n");

    while (1) {
        printk("AEL_ZEPHYR_IDLE count=%u\n", count++);
        k_msleep(500);
    }

    return 0;
}
