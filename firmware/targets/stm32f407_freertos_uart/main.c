/*
 * FreeRTOS UART demo — STM32F407VGT6 (F4 Discovery)
 *
 * Two tasks print alternating lines to USART2 (PA2, 115200, 16 MHz HSI).
 * AEL observe_uart matches:
 *   "STM32F407_FREERTOS_A TICK"
 *   "STM32F407_FREERTOS_B TICK"
 *
 * Hardware: USART2 TX on PA2 → CH341 USB-serial → /dev/ttyUSB0
 *
 * STM32F407 register map (AHB1 peripheral bus):
 *   - GPIOA on AHB1 (0x40020000)
 *   - USART2 on APB1 (0x40004400)
 *   - RCC on AHB1 (0x40023800)
 *   - BRR = 16000000 / 115200 → mantissa=8, fraction=11 → 0x8B = 139
 */

#include <stdint.h>
#include "FreeRTOS.h"
#include "task.h"

/* -----------------------------------------------------------------------
 * STM32F407 register addresses
 * ----------------------------------------------------------------------- */
#define RCC_BASE        0x40023800u
#define RCC_AHB1ENR     (*(volatile uint32_t *)(RCC_BASE + 0x030u))
#define RCC_APB1ENR     (*(volatile uint32_t *)(RCC_BASE + 0x040u))

#define GPIOA_BASE      0x40020000u
#define GPIOA_MODER     (*(volatile uint32_t *)(GPIOA_BASE + 0x00u))
#define GPIOA_OSPEEDR   (*(volatile uint32_t *)(GPIOA_BASE + 0x08u))
#define GPIOA_AFRL      (*(volatile uint32_t *)(GPIOA_BASE + 0x20u))  /* PA0-PA7 */

#define USART2_BASE     0x40004400u
#define USART2_SR       (*(volatile uint32_t *)(USART2_BASE + 0x00u))
#define USART2_DR       (*(volatile uint32_t *)(USART2_BASE + 0x04u))
#define USART2_BRR      (*(volatile uint32_t *)(USART2_BASE + 0x08u))
#define USART2_CR1      (*(volatile uint32_t *)(USART2_BASE + 0x0Cu))

/* -----------------------------------------------------------------------
 * Minimal USART2 driver (polling TX, 16 MHz HSI, 115200 baud)
 * USARTDIV = 16000000 / (16 * 115200) = 8.68
 * mantissa = 8 = 0x8, fraction = round(0.68 * 16) = 11 = 0xB → BRR = 0x8B
 * ----------------------------------------------------------------------- */
static void usart2_init(void)
{
    RCC_AHB1ENR |= (1u << 0);   /* GPIOA clock */
    RCC_APB1ENR |= (1u << 17);  /* USART2 clock */
    (void)RCC_AHB1ENR;
    (void)RCC_APB1ENR;

    /* PA2 = MODER bits[5:4] = 10 (AF), AFRL bits[11:8] = 7 (AF7 = USART2_TX) */
    GPIOA_MODER   = (GPIOA_MODER   & ~(0x3u << 4)) | (0x2u << 4);
    GPIOA_OSPEEDR = (GPIOA_OSPEEDR & ~(0x3u << 4)) | (0x2u << 4);
    GPIOA_AFRL    = (GPIOA_AFRL    & ~(0xFu << 8)) | (0x7u << 8);

    USART2_CR1 = 0u;
    USART2_BRR = 0x8Bu;         /* 115200 @ 16 MHz HSI */
    USART2_CR1 = (1u << 3) | (1u << 13); /* TE=1, UE=1 */
}

static void usart2_putc(char c)
{
    while (!(USART2_SR & (1u << 7))) {} /* wait TXE */
    USART2_DR = (uint8_t)c;
}

static void usart2_puts(const char *s)
{
    while (*s) usart2_putc(*s++);
}

/* -----------------------------------------------------------------------
 * FreeRTOS tasks
 * ----------------------------------------------------------------------- */
static void task_a(void *arg)
{
    (void)arg;
    for (;;) {
        usart2_puts("STM32F407_FREERTOS_A TICK\r\n");
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

static void task_b(void *arg)
{
    (void)arg;
    for (;;) {
        usart2_puts("STM32F407_FREERTOS_B TICK\r\n");
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
 * libc stub — startup_stm32f407xx.s calls __libc_init_array which needs _init
 * ----------------------------------------------------------------------- */
void _init(void) {}

/* -----------------------------------------------------------------------
 * main
 * ----------------------------------------------------------------------- */
int main(void)
{
    usart2_init();
    usart2_puts("STM32F407_FREERTOS SCHEDULER STARTED\r\n");

    xTaskCreate(task_a, "TaskA", configMINIMAL_STACK_SIZE, NULL, 2, NULL);
    xTaskCreate(task_b, "TaskB", configMINIMAL_STACK_SIZE, NULL, 2, NULL);

    vTaskStartScheduler();
    for (;;) {}
}
