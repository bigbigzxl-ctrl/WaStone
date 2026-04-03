#include <stdint.h>

#define AEL_MAILBOX_ADDR 0x20001C00u
#include "../stm32f030c8t6/ael_mailbox.h"

#define RCC_BASE        0x40021000u
#define RCC_AHBENR      (*(volatile uint32_t *)(RCC_BASE + 0x14u))

#define GPIOB_BASE      0x48000400u
#define GPIOB_MODER     (*(volatile uint32_t *)(GPIOB_BASE + 0x00u))
#define GPIOB_PUPDR     (*(volatile uint32_t *)(GPIOB_BASE + 0x0Cu))
#define GPIOB_IDR       (*(volatile uint32_t *)(GPIOB_BASE + 0x10u))
#define GPIOB_ODR       (*(volatile uint32_t *)(GPIOB_BASE + 0x14u))

#define RCC_GPIOBEN     (1u << 18)
#define ERR_HIGH_MISS   0xE101u
#define ERR_LOW_MISS    0xE102u

#define SYSTICK_BASE    0xE000E010u
#define SYST_CSR        (*(volatile uint32_t *)(SYSTICK_BASE + 0x00u))
#define SYST_RVR        (*(volatile uint32_t *)(SYSTICK_BASE + 0x04u))
#define SYST_CVR        (*(volatile uint32_t *)(SYSTICK_BASE + 0x08u))
#define SYST_CSR_ENABLE     (1u << 0)
#define SYST_CSR_CLKSOURCE  (1u << 2)
#define SYST_CSR_COUNTFLAG  (1u << 16)

int main(void)
{
    uint32_t high_seen = 0u;
    uint32_t low_seen = 0u;

    RCC_AHBENR |= RCC_GPIOBEN;

    GPIOB_MODER &= ~((0x3u << 0u) | (0x3u << 2u));
    GPIOB_MODER |= (0x1u << 0u);
    GPIOB_PUPDR &= ~((0x3u << 0u) | (0x3u << 2u));
    GPIOB_PUPDR |= (0x2u << 2u);
    GPIOB_ODR &= ~((1u << 0u) | (1u << 1u));

    SYST_RVR = 7999u;
    SYST_CVR = 0u;
    SYST_CSR = SYST_CSR_CLKSOURCE | SYST_CSR_ENABLE;

    ael_mailbox_init();

    while (1) {
        if ((SYST_CSR & SYST_CSR_COUNTFLAG) == 0u) {
            continue;
        }

        GPIOB_ODR ^= (1u << 0u);
        if ((GPIOB_ODR & (1u << 0u)) != 0u) {
            if ((GPIOB_IDR & (1u << 1u)) == 0u) {
                ael_mailbox_fail(ERR_HIGH_MISS, (high_seen << 16u) | low_seen);
                while (1) {}
            }
            high_seen++;
        } else {
            if ((GPIOB_IDR & (1u << 1u)) != 0u) {
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
