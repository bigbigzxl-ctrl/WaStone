/*
 * AEL Zephyr hello_loop — STM32H563RGT6
 *
 * Prints "AEL_ZEPHYR_IDLE count=N" every 500 ms on USART1/PA9.
 * Console bridged via DAPLink USB CDC → /dev/ttyACM0.
 *
 * Verified on: nucleo_h563zi (DTS overlay → USART1 PA9) / Zephyr 4.x
 */

#include <zephyr/kernel.h>

int main(void)
{
    uint32_t count = 0;

    printk("AEL_ZEPHYR_READY board=stm32h563rgt6\n");

    while (1) {
        printk("AEL_ZEPHYR_IDLE count=%u\n", count++);
        k_msleep(500);
    }

    return 0;
}
