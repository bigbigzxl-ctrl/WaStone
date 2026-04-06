/*
 * CH32V003 GPIO scanner — finds which pin drives the onboard LED
 *
 * Cycles through all available GPIO pins one at a time.
 * Each pin is driven HIGH for ~1 s then LOW for ~0.3 s.
 * When the LED lights up, that is the pin connected to it.
 *
 * Skipped pins: PD1 (SDI/debug), PD7 (NRST)
 *
 * Mailbox detail0 layout:
 *   bits[15:8] = current pin index (0-based, see pin_table below)
 *   bits[7:4]  = port  (0=A, 2=C, 3=D)
 *   bits[3:0]  = pin number within port
 *
 * Pin index → GPIO mapping:
 *   0  = PA1    6  = PC5    12 = PD3
 *   1  = PA2    7  = PC6    13 = PD4
 *   2  = PC0    8  = PC7    14 = PD5
 *   3  = PC1    9  = PD0    15 = PD6
 *   4  = PC2    10 = PD2
 *   5  = PC3    11 = PD4 (skip PD1/PD7)
 *
 * detail0 lets you read current pin via GDB: x/1xw 0x2000060c
 */

#define AEL_MAILBOX_ADDR  0x20000600u

#include "ael_mailbox.h"
#include "ch32v003fun.h"

void SystemInit(void) {}

/* Delay ~1 s at 24 MHz */
static void delay_long(void)
{
    for (volatile uint32_t i = 0; i < 4000000u; i++);
}

/* Delay ~0.3 s */
static void delay_short(void)
{
    for (volatile uint32_t i = 0; i < 1200000u; i++);
}

typedef struct { GPIO_TypeDef *port; uint8_t pin; uint8_t port_id; } PinEntry;

/* All usable pins — skip PD1 (SDI) and PD7 (NRST) */
static const PinEntry pins[] = {
    { GPIOA, 1, 0 }, { GPIOA, 2, 0 },
    { GPIOC, 0, 2 }, { GPIOC, 1, 2 }, { GPIOC, 2, 2 }, { GPIOC, 3, 2 },
    { GPIOC, 4, 2 }, { GPIOC, 5, 2 }, { GPIOC, 6, 2 }, { GPIOC, 7, 2 },
    { GPIOD, 0, 3 }, { GPIOD, 2, 3 }, { GPIOD, 3, 3 },
    { GPIOD, 4, 3 }, { GPIOD, 5, 3 }, { GPIOD, 6, 3 },
};
#define NUM_PINS (sizeof(pins) / sizeof(pins[0]))

int main(void)
{
    ael_mailbox_init();

    /* Enable clocks for GPIOA, GPIOC, GPIOD */
    RCC->APB2PCENR |= RCC_IOPAEN | RCC_IOPCEN | RCC_IOPDEN;

    volatile uint32_t *detail0 = (volatile uint32_t *)(AEL_MAILBOX_ADDR + 12u);

    ael_mailbox_pass();   /* mark PASS so pipeline can attach */

    uint8_t idx = 0;
    while (1)
    {
        const PinEntry *p = &pins[idx];

        /* Configure current pin as push-pull output 50 MHz */
        p->port->CFGLR = (p->port->CFGLR & ~(0xFu << (p->pin * 4)))
                       | (0x3u << (p->pin * 4));

        /* Drive HIGH */
        p->port->BSHR = (1u << p->pin);
        *detail0 = ((uint32_t)idx << 8) | ((uint32_t)p->port_id << 4) | p->pin;
        delay_long();

        /* Drive LOW */
        p->port->BSHR = (1u << (p->pin + 16));
        delay_short();

        /* Reconfigure as floating input before moving to next pin */
        p->port->CFGLR = (p->port->CFGLR & ~(0xFu << (p->pin * 4)))
                       | (0x4u << (p->pin * 4));

        idx = (idx + 1 < NUM_PINS) ? idx + 1 : 0;
    }

    return 0;
}
