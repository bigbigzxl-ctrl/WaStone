/*
 * CH32V003 TIM1 + DMA test — zero wiring
 *
 * TIM1 PWM + DMA Update: DMA1 Channel5 writes a 3-value buffer
 * [10, 50, 80] to TIM1_CH1CVR (0x40012C34) on each TIM1 Update event.
 * DMA runs in Normal mode (3 transfers total).
 *
 * After DMA transfer completes (TC5 flag), CH1CVR should equal the
 * last value (80). This verifies the TIM→DMA trigger chain works.
 *
 * TIM1_CH1CVR address: TIM1_BASE(0x40012C00) + CH1CVR_offset(0x34) = 0x40012C34
 *
 * PASS if DMA TC5 set AND CH1CVR == last buffer value (80).
 * detail0 = final CH1CVR value << 1.
 */

#define AEL_MAILBOX_ADDR  0x20000600u
#include "ael_mailbox.h"
#include "ch32v003fun.h"
#include <stdint.h>

void SystemInit(void) {}

#define TIM1_CH1CVR_ADDR  ((uint32_t)0x40012C34u)
#define BUF_SIZE          3u

/* .bss then filled at runtime — avoid .data initializer (startup bug) */
static uint16_t pbuf[BUF_SIZE];

int main(void)
{
    ael_mailbox_init();

    /* Fill pattern at runtime */
    pbuf[0] = 10u;
    pbuf[1] = 50u;
    pbuf[2] = 80u;

    /* Enable DMA1 + TIM1 clocks */
    RCC->AHBPCENR  |= RCC_DMA1EN;
    RCC->APB2PCENR |= RCC_TIM1EN | RCC_IOPDEN;

    /* Configure PD2 as AF_PP (TIM1_CH1) — needed for TIM1 to function */
    GPIOD->CFGLR = (GPIOD->CFGLR & ~(0xFu << 8)) | (0xBu << 8); /* AF PP 50 MHz */

    /* Reset TIM1 */
    RCC->APB2PRSTR |=  RCC_TIM1RST;
    RCC->APB2PRSTR &= ~RCC_TIM1RST;

    /* TIM1: PSC=23 (1 MHz @ 24 MHz), ARR=100 (10 kHz PWM) */
    TIM1->PSC    = 23u;
    TIM1->ATRLR  = 100u;
    TIM1->CH1CVR = pbuf[0];  /* initial compare value */

    /* CH1: PWM mode 2 (CHCTLR1 bits[6:4]=0b111), OC1PE=1 (bit3) */
    TIM1->CHCTLR1 = (0x7u << 4) | (1u << 3);

    /* Enable CH1 output (CCER bit0) */
    TIM1->CCER = (1u << 0);

    /* UG: load PSC/ARR into shadow registers */
    TIM1->SWEVGR = 0x0001u;
    for (volatile uint32_t i = 0; i < 100u; i++);
    TIM1->INTFR = 0u;

    /* Configure DMA1 Channel5 for TIM1 Update → CH1CVR
     * Direction: MemoryDST (write to TIM1_CH1CVR)
     * Normal mode (not circular), 3 transfers, 16-bit */
    DMA1_Channel5->CFGR  = 0u;
    DMA1_Channel5->PADDR = TIM1_CH1CVR_ADDR;  /* destination = TIM1_CH1CVR */
    DMA1_Channel5->MADDR = (uint32_t)pbuf;     /* source = buffer */
    DMA1_Channel5->CNTR  = BUF_SIZE;
    DMA1_Channel5->CFGR  =
        (3u << 12) |  /* PL = very high */
        (1u << 10) |  /* MSIZE = 16-bit */
        (1u <<  8) |  /* PSIZE = 16-bit */
        (1u <<  7) |  /* MINC */
        (1u <<  4);   /* DIR = 1 (memory → peripheral) */

    /* Clear TC5 */
    DMA1->INTFCR = (1u << 17);  /* CTCIF5 */

    /* Enable DMA channel */
    DMA1_Channel5->CFGR |= (1u << 0);

    /* Enable TIM1 DMA Update request (DMAINTENR bit8 = UDE) */
    TIM1->DMAINTENR |= (1u << 8);

    /* Enable TIM1 and PWM outputs */
    TIM1->CTLR1 = (1u << 0);   /* CEN */
    TIM1->BDTR  = (1u << 15);  /* MOE */

    /* Wait for DMA to complete: TC5 = DMA1->INTFR bit17 */
    uint32_t timeout = 0;
    while (!(DMA1->INTFR & (1u << 17))) {
        if (++timeout > 2000000u) break;
    }

    /* Read final CH1CVR value */
    uint16_t final_cvr = (uint16_t)TIM1->CH1CVR;

    volatile uint32_t *detail0 = (volatile uint32_t *)(AEL_MAILBOX_ADDR + 12u);
    *detail0 = (uint32_t)final_cvr << 1;

    /* PASS if DMA completed and last value written (80) is in CH1CVR */
    if ((DMA1->INTFR & (1u << 17)) && final_cvr == 80u)
        ael_mailbox_pass();
    else
        ael_mailbox_fail(final_cvr, DMA1->INTFR);

    /* Liveness */
    while (1) {
        for (volatile uint32_t i = 0; i < 500000u; i++);
        *detail0 ^= 2u;
    }

    return 0;
}
