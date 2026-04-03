#include <stdint.h>

#define AEL_MAILBOX_ADDR 0x20001C00u
#include "../stm32f030c8t6/ael_mailbox.h"

#define RCC_BASE        0x40021000u
#define RCC_AHBENR      (*(volatile uint32_t *)(RCC_BASE + 0x14u))

#define GPIOC_BASE      0x48000800u
#define GPIOC_MODER     (*(volatile uint32_t *)(GPIOC_BASE + 0x00u))
#define GPIOC_ODR       (*(volatile uint32_t *)(GPIOC_BASE + 0x14u))

#define SYSTICK_BASE    0xE000E010u
#define SYST_CSR        (*(volatile uint32_t *)(SYSTICK_BASE + 0x00u))
#define SYST_RVR        (*(volatile uint32_t *)(SYSTICK_BASE + 0x04u))
#define SYST_CVR        (*(volatile uint32_t *)(SYSTICK_BASE + 0x08u))
#define SYST_CSR_ENABLE     (1u << 0)
#define SYST_CSR_CLKSOURCE  (1u << 2)
#define SYST_CSR_COUNTFLAG  (1u << 16)

#define RCC_GPIOCEN     (1u << 19)

int main(void)
{
    uint32_t settle_ms = 0u;
    uint32_t led_ms = 0u;
    uint32_t heartbeat = 0u;

    RCC_AHBENR |= RCC_GPIOCEN;
    GPIOC_MODER &= ~(0x3u << 26u);
    GPIOC_MODER |=  (0x1u << 26u);
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

        if (++led_ms >= 500u) {
            led_ms = 0u;
            GPIOC_ODR ^= (1u << 13u);
            heartbeat++;
            AEL_MAILBOX->detail0 = heartbeat;
        }
    }
}
