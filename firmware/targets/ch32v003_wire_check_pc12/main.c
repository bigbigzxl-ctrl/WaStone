/*
 * CH32V003 wire connectivity check: PC1 ↔ PC2
 *
 * Prestage test for I2C loopback (SDA=PC1, SCL=PC2).
 * Verifies the PC1↔PC2 loopback wire is present before running I2C loopback test.
 *
 * PC2 = push-pull output (driver)
 * PC1 = floating input  (sense, connected to PC2 externally)
 *
 * Test: drive PC2 HIGH → read PC1 HIGH, drive PC2 LOW → read PC1 LOW.
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

    /* PC1 = floating input          (CFGLR bits[7:4]  = 0x4) */
    /* PC2 = push-pull output 50 MHz (CFGLR bits[11:8] = 0x3) */
    GPIOC->CFGLR = (GPIOC->CFGLR & ~((0xFu << 4) | (0xFu << 8)))
                 | (0x4u << 4)
                 | (0x3u << 8);

    /* Drive PC2 HIGH, settle, read PC1 */
    GPIOC->BSHR = (1u << 2);
    for (volatile uint32_t i = 0; i < 2000u; i++);
    uint32_t high_read = (GPIOC->INDR >> 1) & 1u;

    /* Drive PC2 LOW, settle, read PC1 */
    GPIOC->BSHR = (1u << (2 + 16));
    for (volatile uint32_t i = 0; i < 2000u; i++);
    uint32_t low_read = (GPIOC->INDR >> 1) & 1u;

    uint32_t err = 0u;
    if (!high_read) err |= 1u;  /* expected HIGH, got LOW */
    if ( low_read)  err |= 2u;  /* expected LOW,  got HIGH */

    volatile uint32_t *detail0 = (volatile uint32_t *)(AEL_MAILBOX_ADDR + 12u);
    *detail0 = err << 1;

    if (err == 0u)
        ael_mailbox_pass();
    else
        ael_mailbox_fail(err, (high_read << 1) | low_read);

    /* Liveness: blink PC2 */
    uint32_t toggle = 0u;
    while (1) {
        for (volatile uint32_t i = 0; i < 500000u; i++);
        GPIOC->OUTDR ^= (1u << 2);
        toggle++;
        *detail0 = toggle << 1;
    }

    return 0;
}
