#include <stdint.h>

#define AEL_MAILBOX_ADDR 0x20004C00u
#include "../ael_mailbox.h"

#define RCC_BASE           0x40021000u
#define RCC_APB2ENR        (*(volatile uint32_t *)(RCC_BASE + 0x18u))
#define RCC_IOPAEN         (1u << 2)
#define RCC_USART1EN       (1u << 14)

#define GPIOA_BASE         0x40010800u
#define GPIOA_CRH          (*(volatile uint32_t *)(GPIOA_BASE + 0x04u))

#define USART1_BASE        0x40013800u
#define USART1_SR          (*(volatile uint32_t *)(USART1_BASE + 0x00u))
#define USART1_DR          (*(volatile uint32_t *)(USART1_BASE + 0x04u))
#define USART1_BRR         (*(volatile uint32_t *)(USART1_BASE + 0x08u))
#define USART1_CR1         (*(volatile uint32_t *)(USART1_BASE + 0x0Cu))
#define USART_SR_RXNE      (1u << 5)
#define USART_SR_TXE       (1u << 7)
#define USART_CR1_RE       (1u << 2)
#define USART_CR1_TE       (1u << 3)
#define USART_CR1_UE       (1u << 13)

#define SYST_CSR (*(volatile uint32_t *)0xE000E010u)
#define SYST_RVR (*(volatile uint32_t *)0xE000E014u)
#define SYST_CVR (*(volatile uint32_t *)0xE000E018u)
#define SYST_CSR_ENABLE    (1u << 0)
#define SYST_CSR_CLKSOURCE (1u << 2)
#define SYST_CSR_COUNTFLAG (1u << 16)

#define TX_TIMEOUT 200000u

static const uint8_t TX_BYTES[4] = {0x55u, 0xAAu, 0x12u, 0x34u};

static void delay_ms(uint32_t ms)
{
    for (uint32_t i = 0u; i < ms; i++) {
        SYST_CVR = 0u;
        while ((SYST_CSR & SYST_CSR_COUNTFLAG) == 0u) {}
    }
}

int main(void)
{
    SYST_RVR = 7999u;
    SYST_CVR = 0u;
    SYST_CSR = SYST_CSR_CLKSOURCE | SYST_CSR_ENABLE;

    RCC_APB2ENR |= RCC_IOPAEN | RCC_USART1EN;

    GPIOA_CRH &= ~(0xFFu << 4u);
    GPIOA_CRH |= (0x0Bu << 4u) | (0x04u << 8u);

    USART1_CR1 = 0u;
    USART1_BRR = 0x45u;
    USART1_CR1 = USART_CR1_UE | USART_CR1_TE | USART_CR1_RE;

    ael_mailbox_init();

    uint32_t err = 0u;
    uint32_t matched = 0u;

    for (uint32_t i = 0u; i < 4u; i++) {
        uint32_t timeout = TX_TIMEOUT;
        while ((USART1_SR & USART_SR_RXNE) != 0u && timeout-- > 0u) {
            (void)USART1_DR;
        }

        timeout = TX_TIMEOUT;
        while ((USART1_SR & USART_SR_TXE) == 0u) {
            if (--timeout == 0u) { err |= (1u << i); goto done; }
        }
        USART1_DR = TX_BYTES[i];

        timeout = TX_TIMEOUT;
        while ((USART1_SR & USART_SR_RXNE) == 0u) {
            if (--timeout == 0u) { err |= (1u << i); goto done; }
        }
        if ((uint8_t)USART1_DR != TX_BYTES[i]) {
            err |= (1u << i);
        } else {
            matched++;
        }
    }

done:
    if (err == 0u) {
        ael_mailbox_pass();
        uint32_t iteration = 0u;
        while (1) {
            delay_ms(1u);
            AEL_MAILBOX->detail0 = ++iteration;
        }
    } else {
        ael_mailbox_fail(err, matched);
        while (1) {}
    }
}
