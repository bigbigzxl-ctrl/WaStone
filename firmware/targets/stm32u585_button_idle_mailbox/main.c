#include <stdint.h>
#include "../ael_mailbox.h"

/* RCC */
#define RCC_BASE        0x46020C00u
#define RCC_AHB2ENR1    (*(volatile uint32_t *)(RCC_BASE + 0x8Cu))

/* GPIOA */
#define GPIOA_BASE      0x42020000u
#define GPIOA_MODER     (*(volatile uint32_t *)(GPIOA_BASE + 0x00u))
#define GPIOA_PUPDR     (*(volatile uint32_t *)(GPIOA_BASE + 0x0Cu))
#define GPIOA_IDR       (*(volatile uint32_t *)(GPIOA_BASE + 0x10u))

int main(void) {
    ael_mailbox_init();

    /* Enable GPIOA clock (bit 0 = GPIOAEN) */
    RCC_AHB2ENR1 |= (1u << 0);
    /* Small delay for clock to stabilize */
    volatile uint32_t delay = 10u;
    while (delay--) {}

    /* Set PA0 to input mode (MODER bits[1:0] = 00) */
    GPIOA_MODER &= ~(3u << 0);

    /* Enable internal pull-up on PA0 (PUPDR[1:0] = 01) */
    GPIOA_PUPDR &= ~(3u << 0);
    GPIOA_PUPDR |=  (1u << 0);

    /* Delay for pull-up and input to settle */
    delay = 1000u;
    while (delay--) {}

    /* Read IDR */
    uint32_t idr = GPIOA_IDR;
    AEL_MAILBOX->detail0 = idr;

    /* PA0 should read HIGH (button not pressed, external 510Ω pull-up to 3.3V) */
    if (idr & (1u << 0)) {
        ael_mailbox_pass();
    } else {
        ael_mailbox_fail(0xE001u, idr);
    }

    while (1) {}
    return 0;
}
