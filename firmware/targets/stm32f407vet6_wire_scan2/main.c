/*
 * STM32F407VET6 — Wire scan connectivity test
 *
 * Tests 6 wire pairs via GPIO loopback (output HIGH/LOW → input read).
 * Input pins use pull-down; driver drives to 3.3V or 0V.
 * All 6 must pass for overall PASS.
 *
 * Wire pairs tested:
 *   PB6 (out) → PB7 (in)   bit0  I2C1 wires (new)
 *   PB8 (out) → PB9 (in)   bit1  EXTI wires (new)
 *   PC0 (out) → PC1 (in)   bit2  ADC loopback (new)
 *   PC2 (out) → PC3 (in)   bit3  GPIO loopback (existing)
 *   PA7 (out) → PA6 (in)   bit4  SPI MOSI→MISO (existing)
 *   PD5 (out) → PD6 (in)   bit5  UART loopback (existing, GPIO mode)
 *
 * detail0 on FAIL: bitmask of failed wires (bit = 1 if wire failed)
 * detail0 on PASS: 0x5A5A (sentinel)
 *
 * Clock: 16 MHz HSI. Mailbox: 0x2001FC00.
 */

#include <stdint.h>

#define AEL_MAILBOX_ADDR 0x2001FC00u
#include "ael_mailbox.h"

#define RCC_BASE    0x40023800U
#define RCC_AHB1ENR (*(volatile uint32_t *)(RCC_BASE + 0x30U))

#define GPIOA_BASE  0x40020000U
#define GPIOB_BASE  0x40020400U
#define GPIOC_BASE  0x40020800U
#define GPIOD_BASE  0x40020C00U

#define MODER(b) (*(volatile uint32_t *)((b) + 0x00U))
#define PUPDR(b) (*(volatile uint32_t *)((b) + 0x0CU))
#define IDR(b)   (*(volatile uint32_t *)((b) + 0x10U))
#define BSRR(b)  (*(volatile uint32_t *)((b) + 0x18U))

static void delay_ms(uint32_t ms)
{
    for (uint32_t i = 0U; i < ms; i++) {
        for (volatile uint32_t d = 0U; d < 4000U; d++) {
            __asm__ volatile ("nop");
        }
    }
}

/* Test one wire: drive drv_base pin drv_pin, read in_base pin in_pin.
 * Returns 0 on PASS, 1 on FAIL. */
static uint32_t test_wire(uint32_t drv_base, uint32_t drv_pin,
                           uint32_t in_base,  uint32_t in_pin)
{
    /* driver: output push-pull */
    MODER(drv_base) = (MODER(drv_base) & ~(3U << (drv_pin * 2U)))
                    | (1U << (drv_pin * 2U));

    /* input: input mode, pull-down */
    MODER(in_base) &= ~(3U << (in_pin * 2U));
    PUPDR(in_base)  = (PUPDR(in_base) & ~(3U << (in_pin * 2U)))
                    | (2U << (in_pin * 2U));

    /* Drive HIGH, wait, read */
    BSRR(drv_base) = (1U << drv_pin);
    delay_ms(2U);
    uint32_t hi = (IDR(in_base) >> in_pin) & 1U;

    /* Drive LOW, wait, read */
    BSRR(drv_base) = (1U << (drv_pin + 16U));
    delay_ms(2U);
    uint32_t lo = (IDR(in_base) >> in_pin) & 1U;

    /* PASS: HIGH→1, LOW→0 */
    return (hi == 1U && lo == 0U) ? 0U : 1U;
}

int main(void)
{
    /* Enable GPIOA(0), GPIOB(1), GPIOC(2), GPIOD(3) clocks */
    RCC_AHB1ENR |= 0x0FU;
    (void)RCC_AHB1ENR;

    ael_mailbox_init();

    uint32_t fail = 0U;

    if (test_wire(GPIOB_BASE, 6U, GPIOB_BASE, 7U)) fail |= (1U << 0U);
    AEL_MAILBOX->detail0 = 0x10U | fail;

    if (test_wire(GPIOB_BASE, 8U, GPIOB_BASE, 9U)) fail |= (1U << 1U);
    AEL_MAILBOX->detail0 = 0x20U | fail;

    if (test_wire(GPIOC_BASE, 0U, GPIOC_BASE, 1U)) fail |= (1U << 2U);
    AEL_MAILBOX->detail0 = 0x30U | fail;

    if (test_wire(GPIOC_BASE, 2U, GPIOC_BASE, 3U)) fail |= (1U << 3U);
    AEL_MAILBOX->detail0 = 0x40U | fail;

    if (test_wire(GPIOA_BASE, 7U, GPIOA_BASE, 6U)) fail |= (1U << 4U);
    AEL_MAILBOX->detail0 = 0x50U | fail;

    if (test_wire(GPIOD_BASE, 5U, GPIOD_BASE, 6U)) fail |= (1U << 5U);
    AEL_MAILBOX->detail0 = 0x60U | fail;

    if (fail == 0U) {
        ael_mailbox_pass();
        AEL_MAILBOX->detail0 = 0x5A5AU;
    } else {
        ael_mailbox_fail(fail, 0U);
    }

    while (1) {}
}
