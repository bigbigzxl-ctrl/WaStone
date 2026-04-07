/*
 * CH32V003 OPA (Operational Amplifier) test — zero wiring
 *
 * OPA Channel 0: CHP0 = PA1 (OPP0), CHN0 = PA2 (OPN0), output = PD4.
 *
 * Strategy (zero-wiring):
 *   1. PA1 = input pull-DOWN  (OPA negative input INN0 → ~GND via 40kΩ).
 *   2. PA2 = input pull-UP    (OPA positive input INP0 → ~VCC via 40kΩ).
 *   3. Enable OPA: PSEL=0 (CHP0=PA1), NSEL=0 (CHN0=PA2).
 *   4. OPA output (PD4) should saturate HIGH (V+ > V-).
 *   5. Read PD4 via GPIOD->INDR bit 4 (digital HIGH = OPA saturated).
 *
 * Hardware polarity:
 *   CH32V003 pin description: PA1=OPA_INN0 (negative input), PA2=OPA_INP0 (positive input).
 *   EXTEN_CTR PSEL=0 selects CHP0=PA1 as the P-end route, NSEL=0 selects CHN0=PA2 as
 *   the N-end route.  Despite the PSEL/CHP naming suggesting "positive", the hardware
 *   connects PA1 to the NEGATIVE (inverting) input and PA2 to the POSITIVE (non-inverting)
 *   input, consistent with the pin table (INN0/INP0 suffixes).
 *   → PA1 must be biased LOW (pull-down) and PA2 biased HIGH (pull-up) to get V+ > V-.
 *
 * Settle loop:
 *   Use a SHORT count-down loop (1000 iterations ≈ 166 µs at 24 MHz) so the loop
 *   completes BEFORE WCH OpenOCD 'init' can start (~200 ms process startup).
 *   Count-DOWN (bnez) is also immune to WCH OpenOCD GPR-corruption: the comparison
 *   is against 0 so no register holds the loop limit.
 *
 * PASS if GPIOD INDR bit 4 = 1 (OPA output HIGH).
 * detail0 = pd4 << 1  (bit 1 set = OPA output HIGH)
 */

#define AEL_MAILBOX_ADDR  0x20000600u
#include "ael_mailbox.h"
#include "ch32v003fun.h"
#include <stdint.h>

void SystemInit(void) {}

int main(void)
{
    ael_mailbox_init();

    /* Enable GPIOA + GPIOD + AFIO clocks */
    RCC->APB2PCENR |= RCC_IOPAEN | RCC_IOPDEN | RCC_AFIOEN;

    /* PA1 pull-DOWN → OPA negative input (INN0) ≈ GND
     * PA2 pull-UP   → OPA positive input (INP0) ≈ VCC
     * V+ (PA2) > V- (PA1) → OPA saturates HIGH */
    GPIOA->OUTDR &= ~(1u << 1);                                    /* PA1 LOW → pull-down */
    GPIOA->CFGLR = (GPIOA->CFGLR & ~(0xFu << 4)) | (0x8u << 4);   /* PA1 input-with-pull */
    GPIOA->OUTDR |=  (1u << 2);                                    /* PA2 HIGH → pull-up */
    GPIOA->CFGLR = (GPIOA->CFGLR & ~(0xFu << 8)) | (0x8u << 8);   /* PA2 input-with-pull */

    /* PD4 floating input — let OPA output drive the pin */
    GPIOD->CFGLR = (GPIOD->CFGLR & ~(0xFu << 16)) | (0x4u << 16);

    /* Enable OPA: PSEL=0 (CHP0=PA1 route), NSEL=0 (CHN0=PA2 route), OPA_EN=1 */
    EXTEN->EXTEN_CTR = (EXTEN->EXTEN_CTR & ~(0x7u << 16)) | (0x1u << 16);

    /* Short count-DOWN settle (~166 µs at 24 MHz).
     * Short so the loop finishes before WCH OpenOCD init can race with it.
     * Count-down: bnez only — no register holds the limit (immune to GPR corruption). */
    for (volatile uint32_t i = 1000u; i > 0u; i--);

    /* Read OPA output digitally via GPIOD INDR bit 4 */
    uint32_t pd4 = (GPIOD->INDR >> 4) & 1u;

    volatile uint32_t *detail0 = (volatile uint32_t *)(AEL_MAILBOX_ADDR + 12u);
    *detail0 = ((uint32_t)pd4 << 1);

    if (pd4)
        ael_mailbox_pass();
    else
        ael_mailbox_fail(pd4, 0u);

    /* Liveness: count-down for the same GPR-corruption reason */
    while (1) {
        *detail0 ^= 2u;
        for (volatile uint32_t i = 500000u; i > 0u; i--);
    }

    return 0;
}
