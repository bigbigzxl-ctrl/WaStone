/* ch32v203_wire_check — GPIO continuity check for Stage 1 loopback wiring
 *
 * Checks 4 wire pairs (drive HIGH+LOW, read back):
 *   PA2 (out) ↔ PA3 (in)    bit0  GPIO / EXTI loopback wire
 *   PA9 (out) ↔ PA10 (in)   bit1  UART1 TX→RX wire
 *   PA7 (out) ↔ PA6 (in)    bit2  SPI1 MOSI→MISO wire
 *   PA8 (out) ↔ PA0 (in)    bit3  TIM1_CH1 → TIM2_CH1 wire
 *
 * All input pins use pull-down so floating reads LOW (open = detected as fail).
 * error_code on FAIL: bitmask of failed wires (1 = fail).
 * detail0  on PASS: 0x5A5A (sentinel).
 */
#define AEL_MAILBOX_ADDR 0x20000600u
#include "ael_mailbox.h"
#include "ch32v20x.h"

static void delay_us(uint32_t us)
{
    for (uint32_t i = 0; i < us * 72; i++) __asm__("nop");
}

/* Test one wire: drive drv_pin on GPIOA output, read in_pin on GPIOA input.
 * Returns 0 = PASS, 1 = FAIL. */
static int check_wire_pa(uint8_t drv_pin, uint8_t in_pin)
{
    /* Drive HIGH → expect HIGH */
    GPIOA->BSHR = (1u << drv_pin);
    delay_us(20);
    if (!(GPIOA->INDR & (1u << in_pin))) return 1;

    /* Drive LOW → expect LOW */
    GPIOA->BCR = (1u << drv_pin);
    delay_us(20);
    if (GPIOA->INDR & (1u << in_pin)) return 1;

    return 0;
}

int main(void)
{
    ael_mailbox_init();

    RCC->APB2PCENR |= RCC_APB2Periph_GPIOA;

    /* Configure GPIOA pins:
     * PA2, PA7, PA8, PA9 → Output Push-Pull 2MHz (CFGLR/CFGHR bits = 0x2)
     * PA0, PA3, PA6, PA10 → Input with pull-down (CFGLR/CFGHR bits = 0x8, ODR=0)
     *
     * CFGLR controls PA0..PA7, CFGHR controls PA8..PA15
     * Each pin: 4 bits. CNF[1:0] MODE[1:0]
     *   Output PP 2MHz: MODE=10 CNF=00 → 0x2
     *   Input pull:     MODE=00 CNF=10 → 0x8  (ODR=0 → pull-down)
     */

    /* CFGLR: PA0..PA7
     * PA0(in pull):  bits[3:0]   = 0x8
     * PA2(out PP):   bits[11:8]  = 0x2
     * PA3(in pull):  bits[15:12] = 0x8
     * PA6(in pull):  bits[27:24] = 0x8
     * PA7(out PP):   bits[31:28] = 0x2
     */
    uint32_t cfglr = GPIOA->CFGLR;
    cfglr &= ~((0xFu << 0) | (0xFu << 8) | (0xFu << 12) | (0xFu << 24) | (0xFu << 28));
    cfglr |=  ((0x8u << 0) | (0x2u << 8) | (0x8u << 12) | (0x8u << 24) | (0x2u << 28));
    GPIOA->CFGLR = cfglr;

    /* CFGHR: PA8..PA15
     * PA8(out PP):  bits[3:0]  = 0x2
     * PA9(out PP):  bits[7:4]  = 0x2
     * PA10(in pull):bits[11:8] = 0x8
     */
    uint32_t cfghr = GPIOA->CFGHR;
    cfghr &= ~((0xFu << 0) | (0xFu << 4) | (0xFu << 8));
    cfghr |=  ((0x2u << 0) | (0x2u << 4) | (0x8u << 8));
    GPIOA->CFGHR = cfghr;

    /* ODR = 0 for all pull-down inputs (keep outputs LOW initially) */
    GPIOA->OUTDR &= ~((1u << 0) | (1u << 3) | (1u << 6) | (1u << 10));

    delay_us(50);

    uint32_t fail_mask = 0;

    if (check_wire_pa(2, 3))   fail_mask |= (1u << 0);   /* PA2↔PA3 */
    if (check_wire_pa(9, 10))  fail_mask |= (1u << 1);   /* PA9↔PA10 */
    if (check_wire_pa(7, 6))   fail_mask |= (1u << 2);   /* PA7↔PA6 */
    if (check_wire_pa(8, 0))   fail_mask |= (1u << 3);   /* PA8↔PA0 */

    if (fail_mask) {
        ael_mailbox_fail(fail_mask, 0);
    } else {
        ael_mailbox_pass();
        AEL_MAILBOX->detail0 = 0x5A5A;
    }

    uint32_t tick = 0;
    while (1) {
        /* On PASS: increment detail0 so detail0_increment check sees liveness.
         * On FAIL: keep fail_mask in detail0 for diagnosis. */
        AEL_MAILBOX->detail0 = fail_mask ? fail_mask : ++tick;
        for (volatile int i = 0; i < 720000; i++);
    }
}
