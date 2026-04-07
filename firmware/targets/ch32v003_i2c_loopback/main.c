/*
 * CH32V003 I2C pin loopback test
 *
 * Wire: PC1 ↔ PC2 (I2C SDA=PC1, SCL=PC2).
 *
 * Uses PC2 as push-pull output (driver) and PC1 as floating input
 * (receiver) to verify the PC1↔PC2 wire by looping back a bit pattern.
 *
 * Test flow:
 *   1. Configure PC2 as GPIO PP output, PC1 as floating input.
 *   2. Drive PC2 with 8-bit pattern 0xA5 (10100101), one bit at a time.
 *   3. Read PC1 after each transition, verify it matches driven value.
 *   4. PASS if all 8 bits received correctly.
 *
 * This verifies the PC1↔PC2 wire and I2C-capable pin drive/sense.
 *
 * Note: I2C1 SCL=PC2, SDA=PC1 on CH32V003.
 * detail0 = error_mask (bit per failed bit) << 1.
 */

#define AEL_MAILBOX_ADDR  0x20000600u
#include "ael_mailbox.h"
#include "ch32v003fun.h"
#include <stdint.h>

void SystemInit(void) {}

int main(void)
{
    ael_mailbox_init();

    /* Enable GPIOC clock */
    RCC->APB2PCENR |= RCC_IOPCEN;

    /* PC2 = push-pull output 50 MHz (CFGLR bits[11:8] = 0x3)
     * PC1 = floating input (CFGLR bits[7:4] = 0x4) */
    GPIOC->CFGLR = (GPIOC->CFGLR & ~(0xFFu << 4))
                 | (0x4u << 4)   /* PC1: floating input */
                 | (0x3u << 8);  /* PC2: PP output 50 MHz */

    /* Short settle */
    for (volatile uint32_t i = 0; i < 1000u; i++);

    uint8_t pattern = 0xA5u;  /* 10100101 */
    uint32_t err = 0u;

    for (uint8_t b = 0; b < 8u; b++) {
        uint8_t bit = (pattern >> b) & 1u;

        /* Drive PC2 */
        if (bit)
            GPIOC->BSHR = (1u << 2);       /* PC2 HIGH */
        else
            GPIOC->BSHR = (1u << (2+16));  /* PC2 LOW */

        /* Settle ~10 µs */
        for (volatile uint32_t i = 0; i < 240u; i++);

        /* Read PC1 */
        uint32_t read_bit = (GPIOC->INDR >> 1) & 1u;

        if (read_bit != (uint32_t)bit)
            err |= (1u << b);
    }

    volatile uint32_t *detail0 = (volatile uint32_t *)(AEL_MAILBOX_ADDR + 12u);
    *detail0 = err << 1;

    if (err == 0u)
        ael_mailbox_pass();
    else
        ael_mailbox_fail(err, 0u);

    /* Liveness: keep toggling */
    uint32_t toggle = 0u;
    while (1) {
        GPIOC->BSHR = (toggle & 1u) ? (1u << 2) : (1u << (2+16));
        toggle++;
        for (volatile uint32_t i = 0; i < 100000u; i++);
        *detail0 = toggle << 1;
    }

    return 0;
}
