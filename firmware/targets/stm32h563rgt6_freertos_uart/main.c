/*
 * FreeRTOS UART demo — STM32H563RGT6
 *
 * Two tasks print alternating lines to USART1 (PA9, 115200, 64 MHz HSI).
 * AEL observe_uart matches:
 *   "STM32H563_FREERTOS_A TICK"
 *   "STM32H563_FREERTOS_B TICK"
 *
 * Hardware: USART1 TX on PA9 → DAPLink UART capture → /dev/ttyACM0
 *
 * STM32H563 register map differs from STM32F1:
 *   - GPIOA on AHB2 peripheral bus (0x42020000 non-secure alias)
 *   - USART1 on APB2 (same base 0x40013800)
 *   - GPIO config: MODER + AFRH (not CRH)
 *   - USART ISR/TDR instead of SR/DR
 *   - BRR = 64000000 / 115200 = 555.56 → 556 = 0x22C
 */

#include <stdint.h>
#include "FreeRTOS.h"
#include "task.h"

/* -----------------------------------------------------------------------
 * STM32H563 register addresses
 * ----------------------------------------------------------------------- */
#define RCC_BASE        0x44020C00u
#define RCC_CR          (*(volatile uint32_t *)(RCC_BASE + 0x000u))
#define RCC_AHB2ENR     (*(volatile uint32_t *)(RCC_BASE + 0x08Cu))
#define RCC_APB2ENR     (*(volatile uint32_t *)(RCC_BASE + 0x0A4u))

#define GPIOA_BASE      0x42020000u
#define GPIOA_MODER     (*(volatile uint32_t *)(GPIOA_BASE + 0x00u))
#define GPIOA_OSPEEDR   (*(volatile uint32_t *)(GPIOA_BASE + 0x08u))
#define GPIOA_AFRH      (*(volatile uint32_t *)(GPIOA_BASE + 0x24u))

#define USART1_BASE     0x40013800u
#define USART1_CR1      (*(volatile uint32_t *)(USART1_BASE + 0x00u))
#define USART1_BRR      (*(volatile uint32_t *)(USART1_BASE + 0x0Cu))
#define USART1_ISR      (*(volatile uint32_t *)(USART1_BASE + 0x1Cu))
#define USART1_TDR      (*(volatile uint32_t *)(USART1_BASE + 0x28u))

/* -----------------------------------------------------------------------
 * Minimal USART1 driver (polling TX, 64 MHz HSI, 115200 baud)
 * USARTDIV = 64000000 / (16 * 115200) = 34.72
 * mantissa = 34 = 0x22, fraction = round(0.72 * 16) = 12 = 0xC → BRR = 0x22C
 * ----------------------------------------------------------------------- */
static void usart1_init(void)
{
    RCC_AHB2ENR |= (1u << 0);   /* GPIOA clock */
    RCC_APB2ENR |= (1u << 14);  /* USART1 clock */
    (void)RCC_AHB2ENR;
    (void)RCC_APB2ENR;

    /* PA9 = MODER bits[19:18] = 10 (AF), AFRH bits[7:4] = 7 (AF7 = USART1_TX) */
    GPIOA_MODER   = (GPIOA_MODER   & ~(0x3u << 18)) | (0x2u << 18);
    GPIOA_OSPEEDR = (GPIOA_OSPEEDR & ~(0x3u << 18)) | (0x2u << 18);
    GPIOA_AFRH    = (GPIOA_AFRH    & ~(0xFu <<  4)) | (0x7u <<  4);

    USART1_CR1 = 0u;
    USART1_BRR = 0x22Cu;        /* 115200 @ 64 MHz HSI */
    USART1_CR1 = (1u << 3) | (1u << 0); /* TE=1, UE=1 */
}

static void usart1_putc(char c)
{
    while (!(USART1_ISR & (1u << 7))) {} /* wait TXE */
    USART1_TDR = (uint8_t)c;
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
        usart1_puts("STM32H563_FREERTOS_A TICK\r\n");
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

static void task_b(void *arg)
{
    (void)arg;
    for (;;) {
        usart1_puts("STM32H563_FREERTOS_B TICK\r\n");
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
    /* STM32H5 HSIDIV may be non-zero after pyocd soft-reset (peripheral regs
     * survive VECTRESET). Force HSIDIV=0 → HSI = 64 MHz before UART init.
     * RCC_CR bits[4:3] = HSIDIV; clear to 00b = divide-by-1.
     * Wait for HSIDIVF (bit5) = 0 to confirm divider applied.          */
    RCC_CR &= ~(0x3u << 3);
    while (RCC_CR & (1u << 5)) {}   /* wait HSIDIVF = 0 */

    usart1_init();
    usart1_puts("STM32H563_FREERTOS SCHEDULER STARTED\r\n");

    xTaskCreate(task_a, "TaskA", configMINIMAL_STACK_SIZE, NULL, 2, NULL);
    xTaskCreate(task_b, "TaskB", configMINIMAL_STACK_SIZE, NULL, 2, NULL);

    vTaskStartScheduler();
    for (;;) {}
}
