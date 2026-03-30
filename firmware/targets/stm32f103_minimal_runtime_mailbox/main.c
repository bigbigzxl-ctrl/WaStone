#include <stdint.h>

#define AEL_MAILBOX_ADDR 0x20004C00u
#include "../ael_mailbox.h"

#define RCC_BASE    0x40021000u
#define RCC_APB2ENR (*(volatile uint32_t *)(RCC_BASE + 0x18u))
#define RCC_IOPCEN  (1u << 4)

#define GPIOC_BASE  0x40011000u
#define GPIOC_CRH   (*(volatile uint32_t *)(GPIOC_BASE + 0x04u))
#define GPIOC_ODR   (*(volatile uint32_t *)(GPIOC_BASE + 0x0Cu))

#define SYSTICK_BASE 0xE000E010u
#define SYST_CSR (*(volatile uint32_t *)(SYSTICK_BASE + 0x00u))
#define SYST_RVR (*(volatile uint32_t *)(SYSTICK_BASE + 0x04u))
#define SYST_CVR (*(volatile uint32_t *)(SYSTICK_BASE + 0x08u))

#define SYST_CSR_ENABLE     (1u << 0)
#define SYST_CSR_CLKSOURCE  (1u << 2)
#define SYST_CSR_COUNTFLAG  (1u << 16)

int main(void) {
    uint32_t settle_ms = 0u;
    uint32_t led_ms = 0u;
    uint32_t heartbeat = 0u;

    RCC_APB2ENR |= RCC_IOPCEN;

    GPIOC_CRH &= ~(0xFu << 20u);
    GPIOC_CRH |=  (0x2u << 20u);
    GPIOC_ODR |= (1u << 13u);

    SYST_RVR = 7999u;
    SYST_CVR = 0u;
    SYST_CSR = SYST_CSR_CLKSOURCE | SYST_CSR_ENABLE;

    ael_mailbox_init();

    while (1) {
        if ((SYST_CSR & SYST_CSR_COUNTFLAG) == 0u) {
            continue;
        }

        if (AEL_MAILBOX->status == AEL_STATUS_RUNNING) {
            if (++settle_ms >= 50u) {
                ael_mailbox_pass();
                heartbeat = 1u;
                AEL_MAILBOX->detail0 = heartbeat;
            }
            continue;
        }

        if (++led_ms >= 1000u) {
            led_ms = 0u;
            GPIOC_ODR ^= (1u << 13u);
            heartbeat++;
            AEL_MAILBOX->detail0 = heartbeat;
        }
    }
}
