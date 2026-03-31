/*
 * STM32F103RCT6 — USART1 DMA TX proof
 *
 * Observable behaviour:
 *   - DMA1 Channel4 feeds USART1_DR for a repeated frame on PA9
 *   - Host UART observer should see: "AEL_UART_DMA A1 B2 C3 D4 55 66 77 88"
 *   - Mailbox PASS after the first DMA transfer completes
 *   - detail0 = transmitted frame count
 *
 * Wiring required:
 *   - PA9/USART1_TX -> DAPLink RX
 *   - DAPLink TX -> PA10/USART1_RX (present but unused by this proof)
 * Mailbox address: 0x2000BC00
 */

#include <stdint.h>

#define AEL_MAILBOX_ADDR 0x2000BC00u
#include "ael_mailbox.h"

#define RCC_BASE        0x40021000U
#define RCC_AHBENR      (*(volatile uint32_t *)(RCC_BASE + 0x14U))
#define RCC_APB2ENR     (*(volatile uint32_t *)(RCC_BASE + 0x18U))

#define GPIOA_BASE      0x40010800U
#define GPIOA_CRH       (*(volatile uint32_t *)(GPIOA_BASE + 0x04U))

#define USART1_BASE     0x40013800U
#define USART1_SR       (*(volatile uint32_t *)(USART1_BASE + 0x00U))
#define USART1_DR       (*(volatile uint32_t *)(USART1_BASE + 0x04U))
#define USART1_BRR      (*(volatile uint32_t *)(USART1_BASE + 0x08U))
#define USART1_CR1      (*(volatile uint32_t *)(USART1_BASE + 0x0CU))
#define USART1_CR3      (*(volatile uint32_t *)(USART1_BASE + 0x14U))

#define DMA1_BASE       0x40020000U
#define DMA1_ISR        (*(volatile uint32_t *)(DMA1_BASE + 0x00U))
#define DMA1_IFCR       (*(volatile uint32_t *)(DMA1_BASE + 0x04U))
#define DMA1_CCR4       (*(volatile uint32_t *)(DMA1_BASE + 0x44U))
#define DMA1_CNDTR4     (*(volatile uint32_t *)(DMA1_BASE + 0x48U))
#define DMA1_CPAR4      (*(volatile uint32_t *)(DMA1_BASE + 0x4CU))
#define DMA1_CMAR4      (*(volatile uint32_t *)(DMA1_BASE + 0x50U))

#define USART_SR_TC     (1U << 6)
#define USART_CR3_DMAT  (1U << 7)

#define DMA_CCR_EN      (1U << 0)
#define DMA_CCR_DIR     (1U << 4)
#define DMA_CCR_MINC    (1U << 7)
#define DMA_ISR_TCIF4   (1U << 13)
#define DMA_ISR_TEIF4   (1U << 15)
#define DMA_IFCR_ALL4   (0x0FU << 12U)

#define DMA_TIMEOUT     2000000U

static const uint8_t TX_FRAME[] =
    "AEL_UART_DMA A1 B2 C3 D4 55 66 77 88\r\n";

static void delay(volatile uint32_t n)
{
    while (n--) __asm__ volatile ("nop");
}

static uint32_t run_dma_frame(void)
{
    uint32_t timeout = DMA_TIMEOUT;

    DMA1_CCR4 = 0U;
    DMA1_IFCR = DMA_IFCR_ALL4;
    DMA1_CPAR4 = (uint32_t)&USART1_DR;
    DMA1_CMAR4 = (uint32_t)TX_FRAME;
    DMA1_CNDTR4 = (uint32_t)(sizeof(TX_FRAME) - 1U);
    DMA1_CCR4 = DMA_CCR_DIR | DMA_CCR_MINC | DMA_CCR_EN;

    while (((DMA1_ISR & (DMA_ISR_TCIF4 | DMA_ISR_TEIF4)) == 0U) && timeout--) {}
    if ((DMA1_ISR & DMA_ISR_TEIF4) != 0U) {
        return 0xD401U;
    }
    if ((DMA1_ISR & DMA_ISR_TCIF4) == 0U) {
        return 0xD402U;
    }

    while ((USART1_SR & USART_SR_TC) == 0U) {}
    DMA1_CCR4 = 0U;
    DMA1_IFCR = DMA_IFCR_ALL4;
    return 0U;
}

int main(void)
{
    RCC_AHBENR |= (1U << 0);
    RCC_APB2ENR |= (1U << 2) | (1U << 14);

    /*
     * PA9  (TX): AF push-pull 50 MHz -> CRH[7:4]  = 0xB
     * PA10 (RX): input floating      -> CRH[11:8] = 0x4
     */
    GPIOA_CRH &= ~0xFF0U;
    GPIOA_CRH |=  0x4B0U;

    USART1_BRR = 0x45U;
    USART1_CR3 = USART_CR3_DMAT;
    USART1_CR1 = (1U << 13) | (1U << 3) | (1U << 2);

    ael_mailbox_init();

    uint32_t frames = 0U;

    while (1) {
        uint32_t err = run_dma_frame();
        if (err != 0U) {
            ael_mailbox_fail(err, frames);
            while (1) {}
        }

        frames++;
        AEL_MAILBOX->detail0 = frames;
        if (frames == 1U) {
            ael_mailbox_pass();
        }

        delay(240000U);
    }
}
