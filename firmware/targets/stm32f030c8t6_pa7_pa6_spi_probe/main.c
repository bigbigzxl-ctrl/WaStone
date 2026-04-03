#include <stdint.h>

#define AEL_MAILBOX_ADDR 0x20001C00u
#include "../stm32f030c8t6/ael_mailbox.h"

#define RCC_BASE        0x40021000u
#define RCC_AHBENR      (*(volatile uint32_t *)(RCC_BASE + 0x14u))

#define GPIOA_BASE      0x48000000u
#define GPIOA_MODER     (*(volatile uint32_t *)(GPIOA_BASE + 0x00u))
#define GPIOA_PUPDR     (*(volatile uint32_t *)(GPIOA_BASE + 0x0Cu))
#define GPIOA_IDR       (*(volatile uint32_t *)(GPIOA_BASE + 0x10u))
#define GPIOA_ODR       (*(volatile uint32_t *)(GPIOA_BASE + 0x14u))

#define RCC_GPIOAEN     (1u << 17)
#define ERR_HIGH_MISS   0xE601u
#define ERR_LOW_MISS    0xE602u

#define SYSTICK_BASE        0xE000E010u
#define SYST_CSR            (*(volatile uint32_t *)(SYSTICK_BASE + 0x00u))
#define SYST_RVR            (*(volatile uint32_t *)(SYSTICK_BASE + 0x04u))
#define SYST_CVR            (*(volatile uint32_t *)(SYSTICK_BASE + 0x08u))
#define SYST_CSR_ENABLE     (1u << 0)
#define SYST_CSR_CLKSOURCE  (1u << 2)
#define SYST_CSR_COUNTFLAG  (1u << 16)

static void delay_cycles(volatile uint32_t n)
{
    while (n-- > 0u) {
        __asm__ volatile ("nop");
    }
}

int main(void)
{
    uint32_t high_seen = 0u;
    uint32_t low_seen = 0u;

    RCC_AHBENR |= RCC_GPIOAEN;

    GPIOA_MODER &= ~((0x3u << 12u) | (0x3u << 14u));
    GPIOA_MODER |=  (0x1u << 14u);
    GPIOA_PUPDR &= ~((0x3u << 12u) | (0x3u << 14u));
    GPIOA_PUPDR |=  (0x2u << 12u);
    GPIOA_ODR &= ~((1u << 6u) | (1u << 7u));

    SYST_RVR = 7999u;
    SYST_CVR = 0u;
    SYST_CSR = SYST_CSR_CLKSOURCE | SYST_CSR_ENABLE;

    ael_mailbox_init();

    while (1) {
        if ((SYST_CSR & SYST_CSR_COUNTFLAG) == 0u) {
            continue;
        }

        GPIOA_ODR ^= (1u << 7u);
        delay_cycles(64u);
        if ((GPIOA_ODR & (1u << 7u)) != 0u) {
            if ((GPIOA_IDR & (1u << 6u)) == 0u) {
                ael_mailbox_fail(ERR_HIGH_MISS, (high_seen << 16u) | low_seen);
                while (1) {}
            }
            high_seen++;
        } else {
            if ((GPIOA_IDR & (1u << 6u)) != 0u) {
                ael_mailbox_fail(ERR_LOW_MISS, (high_seen << 16u) | low_seen);
                while (1) {}
            }
            low_seen++;
        }

        AEL_MAILBOX->detail0 = (high_seen << 16u) | low_seen;
        if (high_seen >= 4u && low_seen >= 4u) {
            ael_mailbox_pass();
            while (1) {
                if ((SYST_CSR & SYST_CSR_COUNTFLAG) == 0u) {
                    continue;
                }
                AEL_MAILBOX->detail0++;
            }
        }
    }
}
