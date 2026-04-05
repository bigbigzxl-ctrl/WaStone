/*
 * FreeRTOS UART demo — STM32F103RCT6
 *
 * Two tasks print alternating lines to USART1 (PA9, 115200, 8 MHz HSI).
 * AEL observe_uart matches:
 *   "AEL_FREERTOS_A TICK"
 *   "AEL_FREERTOS_B TICK"
 *   "AEL_FREERTOS SCHEDULER STARTED"
 *
 * Hardware: USART1 TX on PA9 → DAPLink UART capture → /dev/ttyACM0
 */

#include <stdint.h>
#include "FreeRTOS.h"
#include "task.h"

/* -----------------------------------------------------------------------
 * STM32F103 register addresses
 * ----------------------------------------------------------------------- */
#define RCC_BASE    0x40021000UL
#define RCC_APB2ENR (*(volatile uint32_t *)(RCC_BASE + 0x18))

#define GPIOA_BASE  0x40010800UL
#define GPIOA_CRH   (*(volatile uint32_t *)(GPIOA_BASE + 0x04))

#define USART1_BASE 0x40013800UL
#define USART1_SR   (*(volatile uint32_t *)(USART1_BASE + 0x00))
#define USART1_DR   (*(volatile uint32_t *)(USART1_BASE + 0x04))
#define USART1_BRR  (*(volatile uint32_t *)(USART1_BASE + 0x08))
#define USART1_CR1  (*(volatile uint32_t *)(USART1_BASE + 0x0C))

/* -----------------------------------------------------------------------
 * Minimal USART1 driver (polling TX, 8 MHz HSI, 115200 baud)
 * USARTDIV = 8000000 / (16 × 115200) = 4.34 → mantissa=4, fraction=5 → BRR=0x45
 * ----------------------------------------------------------------------- */
static void usart1_init(void)
{
    RCC_APB2ENR |= (1u << 2) | (1u << 14); /* GPIOA + USART1 clocks */

    /* PA9: AF push-pull output, 50 MHz (CNF=10, MODE=11 → 0xB) */
    GPIOA_CRH = (GPIOA_CRH & ~(0xFu << 4)) | (0xBu << 4);

    USART1_BRR = 0x45u;                 /* 115200 @ 8 MHz: USARTDIV=4.3125, BRR=(4<<4)|5 */
    USART1_CR1 = (1u << 3) | (1u << 13); /* TE=1, UE=1 */
}

static void usart1_putc(char c)
{
    while (!(USART1_SR & (1u << 7))) {} /* wait TXE */
    USART1_DR = (uint8_t)c;
}

static void usart1_puts(const char *s)
{
    while (*s) usart1_putc(*s++);
}

/* -----------------------------------------------------------------------
 * FreeRTOS tasks
 * ----------------------------------------------------------------------- */
static void task_a(void *arg)
{
    (void)arg;
    for (;;) {
        usart1_puts("AEL_FREERTOS_A TICK\r\n");
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

static void task_b(void *arg)
{
    (void)arg;
    for (;;) {
        usart1_puts("AEL_FREERTOS_B TICK\r\n");
        vTaskDelay(pdMS_TO_TICKS(700));
    }
}

/* -----------------------------------------------------------------------
 * HardFault handler — SYSRESETREQ to avoid SWD lockup
 * ----------------------------------------------------------------------- */
void HardFault_Handler(void)
{
    volatile uint32_t *aircr = (volatile uint32_t *)0xE000ED0Cu;
    *aircr = 0x05FA0004u;
    while (1) {}
}

/* -----------------------------------------------------------------------
 * main
 * ----------------------------------------------------------------------- */
int main(void)
{
    usart1_init();
    usart1_puts("AEL_FREERTOS SCHEDULER STARTED\r\n");

    xTaskCreate(task_a, "TaskA", configMINIMAL_STACK_SIZE, NULL, 2, NULL);
    xTaskCreate(task_b, "TaskB", configMINIMAL_STACK_SIZE, NULL, 2, NULL);

    vTaskStartScheduler();
    /* Should never reach here */
    for (;;) {}
}
