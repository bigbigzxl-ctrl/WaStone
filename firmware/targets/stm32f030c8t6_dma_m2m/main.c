#include <stdint.h>

#define AEL_MAILBOX_ADDR 0x20001C00u
#include "../stm32f030c8t6/ael_mailbox.h"

#define RCC_BASE        0x40021000u
#define RCC_AHBENR      (*(volatile uint32_t *)(RCC_BASE + 0x14u))

#define DMA1_BASE       0x40020000u
#define DMA1_ISR        (*(volatile uint32_t *)(DMA1_BASE + 0x00u))
#define DMA1_IFCR       (*(volatile uint32_t *)(DMA1_BASE + 0x04u))
#define DMA1_CCR1       (*(volatile uint32_t *)(DMA1_BASE + 0x08u))
#define DMA1_CNDTR1     (*(volatile uint32_t *)(DMA1_BASE + 0x0Cu))
#define DMA1_CPAR1      (*(volatile uint32_t *)(DMA1_BASE + 0x10u))
#define DMA1_CMAR1      (*(volatile uint32_t *)(DMA1_BASE + 0x14u))

#define RCC_DMA1EN      (1u << 0)

#define DMA_CCR_EN        (1u << 0)
#define DMA_CCR_PINC      (1u << 6)
#define DMA_CCR_MINC      (1u << 7)
#define DMA_CCR_PSIZE32   (0x2u << 8)
#define DMA_CCR_MSIZE32   (0x2u << 10)
#define DMA_CCR_MEM2MEM   (1u << 14)

#define DMA_ISR_TCIF1     (1u << 1)
#define DMA_ISR_TEIF1     (1u << 3)
#define DMA_IFCR_ALL1     (0x0Fu << 0)

#define WORDS   8u
#define TIMEOUT 1000000u

static uint32_t src[WORDS];
static uint32_t dst[WORDS];

int main(void)
{
    RCC_AHBENR |= RCC_DMA1EN;
    (void)RCC_AHBENR;

    ael_mailbox_init();

    for (uint32_t i = 0u; i < WORDS; ++i) {
        src[i] = 0xA5A50000u | i;
        dst[i] = 0u;
    }

    DMA1_CCR1 = 0u;
    DMA1_IFCR = DMA_IFCR_ALL1;
    DMA1_CPAR1 = (uint32_t)src;
    DMA1_CMAR1 = (uint32_t)dst;
    DMA1_CNDTR1 = WORDS;
    DMA1_CCR1 = DMA_CCR_MEM2MEM | DMA_CCR_PINC | DMA_CCR_MINC | DMA_CCR_PSIZE32 | DMA_CCR_MSIZE32;
    DMA1_CCR1 |= DMA_CCR_EN;

    {
        uint32_t timeout = TIMEOUT;
        while ((DMA1_ISR & (DMA_ISR_TCIF1 | DMA_ISR_TEIF1)) == 0u) {
            if (--timeout == 0u) {
                ael_mailbox_fail(0xD601u, DMA1_CNDTR1);
                while (1) {}
            }
        }
    }

    if ((DMA1_ISR & DMA_ISR_TEIF1) != 0u) {
        ael_mailbox_fail(0xD602u, DMA1_ISR);
        while (1) {}
    }

    for (uint32_t i = 0u; i < WORDS; ++i) {
        if (dst[i] != src[i]) {
            ael_mailbox_fail(0xD603u, i);
            while (1) {}
        }
    }

    AEL_MAILBOX->detail0 = WORDS;
    ael_mailbox_pass();

    while (1) {}
}
