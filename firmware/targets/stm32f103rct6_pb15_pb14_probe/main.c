#include <stdint.h>

#define AEL_MAILBOX_ADDR 0x2000BC00u
#include "../stm32f103rct6/ael_mailbox.h"

#define RCC_BASE        0x40021000U
#define RCC_APB2ENR     (*(volatile uint32_t *)(RCC_BASE + 0x18U))

#define GPIOB_BASE      0x40010C00U
#define GPIOB_CRH       (*(volatile uint32_t *)(GPIOB_BASE + 0x04U))
#define GPIOB_IDR       (*(volatile uint32_t *)(GPIOB_BASE + 0x08U))
#define GPIOB_ODR       (*(volatile uint32_t *)(GPIOB_BASE + 0x0CU))

#define ERR_HIGH_MISS 0xE301u
#define ERR_LOW_MISS  0xE302u

#define SYSTICK_BASE 0xE000E010u
#define SYST_CSR (*(volatile uint32_t *)(SYSTICK_BASE + 0x00u))
#define SYST_RVR (*(volatile uint32_t *)(SYSTICK_BASE + 0x04u))
#define SYST_CVR (*(volatile uint32_t *)(SYSTICK_BASE + 0x08u))
#define SYST_CSR_ENABLE     (1u << 0)
#define SYST_CSR_CLKSOURCE  (1u << 2)
#define SYST_CSR_COUNTFLAG  (1u << 16)

int main(void)
{
    uint32_t high_seen = 0u;
    uint32_t low_seen = 0u;

    RCC_APB2ENR |= (1u << 3);

    /*
     * PB15 output push-pull 2 MHz -> CRH[31:28] = 0x2
     * PB14 input pull-down        -> CRH[27:24] = 0x8, ODR14 = 0
     */
    GPIOB_CRH &= ~((0xFu << 24u) | (0xFu << 28u));
    GPIOB_CRH |= (0x8u << 24u) | (0x2u << 28u);
    GPIOB_ODR &= ~((1u << 14u) | (1u << 15u));

    SYST_RVR = 7999u;
    SYST_CVR = 0u;
    SYST_CSR = SYST_CSR_CLKSOURCE | SYST_CSR_ENABLE;

    ael_mailbox_init();

    while (1) {
        if ((SYST_CSR & SYST_CSR_COUNTFLAG) == 0u) {
            continue;
        }

        GPIOB_ODR ^= (1u << 15u);
        if (GPIOB_ODR & (1u << 15u)) {
            if ((GPIOB_IDR & (1u << 14u)) == 0u) {
                ael_mailbox_fail(ERR_HIGH_MISS, (high_seen << 16u) | low_seen);
                while (1) {}
            }
            high_seen++;
        } else {
            if (GPIOB_IDR & (1u << 14u)) {
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
