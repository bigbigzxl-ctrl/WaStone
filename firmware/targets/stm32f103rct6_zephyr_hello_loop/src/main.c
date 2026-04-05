/*
 * AEL Zephyr hello_loop — STM32F103RCT6 (stm32f103_mini board)
 *
 * Console: USART1 / PA9 (TX) at 115200 8N1
 * UART capture: DAPLink built-in UART bridge, PA9 → DAPLink RX → /dev/ttyACM0
 * Board:   stm32f103_mini (STM32F103RCT6, 256 KB Flash)
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
