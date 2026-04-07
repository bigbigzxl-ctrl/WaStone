/*
 * CH32V003 wire connectivity check: PC6 ↔ PC7
 *
 * Prestage test for SPI DMA (MOSI=PC6, MISO=PC7).
 * Verifies the PC6↔PC7 loopback wire is present before running SPI DMA test.
 *
 * PC6 = push-pull output (driver)
 * PC7 = floating input  (sense, connected to PC6 externally)
 *
 * Test: drive PC6 HIGH → read PC7 HIGH, drive PC6 LOW → read PC7 LOW.
 * PASS if both match. detail0 = err << 1 (bit0=high_fail, bit1=low_fail).
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

    /* PC6 = push-pull output 50 MHz (CFGLR bits[27:24] = 0x3) */
    /* PC7 = floating input          (CFGLR bits[31:28] = 0x4) */
    GPIOC->CFGLR = (GPIOC->CFGLR & ~((0xFu << 24) | (0xFu << 28)))
                 | (0x3u << 24)
                 | (0x4u << 28);

    /* Drive PC6 HIGH, settle, read PC7 */
    GPIOC->BSHR = (1u << 6);
    for (volatile uint32_t i = 0; i < 2000u; i++);
    uint32_t high_read = (GPIOC->INDR >> 7) & 1u;

    /* Drive PC6 LOW, settle, read PC7 */
    GPIOC->BSHR = (1u << (6 + 16));
    for (volatile uint32_t i = 0; i < 2000u; i++);
    uint32_t low_read = (GPIOC->INDR >> 7) & 1u;

    uint32_t err = 0u;
    if (!high_read) err |= 1u;  /* expected HIGH, got LOW */
    if ( low_read)  err |= 2u;  /* expected LOW,  got HIGH */

    volatile uint32_t *detail0 = (volatile uint32_t *)(AEL_MAILBOX_ADDR + 12u);
    *detail0 = err << 1;

    if (err == 0u)
        ael_mailbox_pass();
    else
        ael_mailbox_fail(err, (high_read << 1) | low_read);

    /* Liveness: blink PC6 */
    uint32_t toggle = 0u;
    while (1) {
        for (volatile uint32_t i = 0; i < 500000u; i++);
        GPIOC->OUTDR ^= (1u << 6);
        toggle++;
        *detail0 = toggle << 1;
    }

    return 0;
}
