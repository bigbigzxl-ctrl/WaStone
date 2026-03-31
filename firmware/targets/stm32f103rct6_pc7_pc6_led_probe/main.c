#include <stdint.h>

#define AEL_MAILBOX_ADDR 0x2000BC00u
#include "../stm32f103rct6/ael_mailbox.h"

#define RCC_BASE    0x40021000u
#define RCC_APB2ENR (*(volatile uint32_t *)(RCC_BASE + 0x18u))
#define RCC_IOPCEN  (1u << 4)

#define GPIOC_BASE  0x40011000u
#define GPIOC_CRH   (*(volatile uint32_t *)(GPIOC_BASE + 0x04u))
#define GPIOC_IDR   (*(volatile uint32_t *)(GPIOC_BASE + 0x08u))
#define GPIOC_ODR   (*(volatile uint32_t *)(GPIOC_BASE + 0x0Cu))

#define SYSTICK_BASE 0xE000E010u
#define SYST_CSR (*(volatile uint32_t *)(SYSTICK_BASE + 0x00u))
#define SYST_RVR (*(volatile uint32_t *)(SYSTICK_BASE + 0x04u))
#define SYST_CVR (*(volatile uint32_t *)(SYSTICK_BASE + 0x08u))

#define SYST_CSR_ENABLE     (1u << 0)
#define SYST_CSR_CLKSOURCE  (1u << 2)
#define SYST_CSR_COUNTFLAG  (1u << 16)

#define ERR_HIGH_MISS 0xE701u
#define ERR_LOW_MISS  0xE702u

int main(void)
{
    uint32_t toggles = 0u;
    uint32_t high_seen = 0u;
    uint32_t low_seen = 0u;

    RCC_APB2ENR |= RCC_IOPCEN;

    /*
     * PC6: input pull-down -> CRH[11:8] = 0x8, ODR6 = 0
     * PC7: output push-pull 2 MHz -> CRH[15:12] = 0x2
     */
    GPIOC_CRH &= ~(0xFFu << 8u);
    GPIOC_CRH |=  (0x28u << 8u);
    GPIOC_ODR &= ~((1u << 6u) | (1u << 7u));

    SYST_RVR = 7999u;
    SYST_CVR = 0u;
    SYST_CSR = SYST_CSR_CLKSOURCE | SYST_CSR_ENABLE;

    ael_mailbox_init();

    while (1) {
        if ((SYST_CSR & SYST_CSR_COUNTFLAG) == 0u) {
            continue;
        }

        toggles++;
        GPIOC_ODR ^= (1u << 7u);

        if (GPIOC_ODR & (1u << 7u)) {
            if ((GPIOC_IDR & (1u << 6u)) == 0u) {
                ael_mailbox_fail(ERR_HIGH_MISS, toggles);
                while (1) {}
            }
            high_seen++;
        } else {
            if (GPIOC_IDR & (1u << 6u)) {
                ael_mailbox_fail(ERR_LOW_MISS, toggles);
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
