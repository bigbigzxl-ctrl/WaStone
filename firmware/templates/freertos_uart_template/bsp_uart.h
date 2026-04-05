/*
 * bsp_uart.h — Board Support Package interface for AEL FreeRTOS UART demo.
 *
 * Each board target provides a bsp_uart.c implementing these two functions.
 * tasks_main.c uses only this interface — no board-specific code there.
 */

#ifndef BSP_UART_H
#define BSP_UART_H

/* Initialise the UART peripheral (GPIO, clocks, baud rate, TX enable).
   Called once from main() before the scheduler starts. */
void bsp_uart_init(void);

/* Transmit a null-terminated string (polling TX). */
void bsp_uart_puts(const char *s);

#endif /* BSP_UART_H */
