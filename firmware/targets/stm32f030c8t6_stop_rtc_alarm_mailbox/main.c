#include <stdint.h>

#define AEL_MAILBOX_ADDR 0x20001C00u
#include "../stm32f030c8t6/ael_mailbox.h"

#define RCC_BASE        0x40021000u
#define RCC_APB1ENR     (*(volatile uint32_t *)(RCC_BASE + 0x1Cu))
#define RCC_BDCR        (*(volatile uint32_t *)(RCC_BASE + 0x20u))
#define RCC_CSR         (*(volatile uint32_t *)(RCC_BASE + 0x24u))

#define RCC_APB1ENR_PWREN (1u << 28)
#define RCC_BDCR_RTCSEL_LSI (2u << 8)
#define RCC_BDCR_RTCEN   (1u << 15)
#define RCC_BDCR_BDRST   (1u << 16)
#define RCC_CSR_LSION    (1u << 0)
#define RCC_CSR_LSIRDY   (1u << 1)

#define PWR_BASE        0x40007000u
#define PWR_CR          (*(volatile uint32_t *)(PWR_BASE + 0x00u))
#define PWR_CSR         (*(volatile uint32_t *)(PWR_BASE + 0x04u))
#define PWR_CR_LPDS     (1u << 0)
#define PWR_CR_PDDS     (1u << 1)
#define PWR_CR_CWUF     (1u << 2)
#define PWR_CR_DBP      (1u << 8)

#define RTC_BASE        0x40002800u
#define RTC_TR          (*(volatile uint32_t *)(RTC_BASE + 0x00u))
#define RTC_DR          (*(volatile uint32_t *)(RTC_BASE + 0x04u))
#define RTC_CR          (*(volatile uint32_t *)(RTC_BASE + 0x08u))
#define RTC_ISR         (*(volatile uint32_t *)(RTC_BASE + 0x0Cu))
#define RTC_PRER        (*(volatile uint32_t *)(RTC_BASE + 0x10u))
#define RTC_ALRMAR      (*(volatile uint32_t *)(RTC_BASE + 0x1Cu))
#define RTC_WPR         (*(volatile uint32_t *)(RTC_BASE + 0x24u))

#define RTC_CR_ALRAE    (1u << 8)
#define RTC_CR_ALRAIE   (1u << 12)
#define RTC_ISR_ALRAF   (1u << 8)
#define RTC_ISR_INIT    (1u << 7)
#define RTC_ISR_INITF   (1u << 6)
#define RTC_ISR_RSF     (1u << 5)
#define RTC_ISR_ALRAWF  (1u << 0)

#define EXTI_BASE       0x40010400u
#define EXTI_IMR        (*(volatile uint32_t *)(EXTI_BASE + 0x00u))
#define EXTI_RTSR       (*(volatile uint32_t *)(EXTI_BASE + 0x08u))
#define EXTI_PR         (*(volatile uint32_t *)(EXTI_BASE + 0x14u))
#define EXTI_LINE_RTC_ALARM (1u << 17)

#define SCB_SCR         (*(volatile uint32_t *)0xE000ED10u)
#define SCB_SCR_SLEEPDEEP (1u << 2)
#define NVIC_ISER       (*(volatile uint32_t *)0xE000E100u)
#define RTC_IRQn_BIT    (1u << 2)

static volatile uint32_t g_alarm_fired = 0u;

void RTC_IRQHandler(void)
{
    if (RTC_ISR & RTC_ISR_ALRAF) {
        g_alarm_fired = 1u;
    }
    EXTI_PR = EXTI_LINE_RTC_ALARM;
}

static uint32_t wait_until_set(volatile uint32_t *reg, uint32_t mask, uint32_t timeout)
{
    while (((*reg) & mask) == 0u) {
        if (--timeout == 0u) {
            return 0u;
        }
    }
    return 1u;
}

int main(void)
{
    ael_mailbox_init();

    RCC_APB1ENR |= RCC_APB1ENR_PWREN;
    (void)RCC_APB1ENR;

    PWR_CR |= PWR_CR_DBP;
    (void)PWR_CR;

    RCC_BDCR |= RCC_BDCR_BDRST;
    RCC_BDCR &= ~RCC_BDCR_BDRST;

    RCC_CSR |= RCC_CSR_LSION;
    if (!wait_until_set((volatile uint32_t *)&RCC_CSR, RCC_CSR_LSIRDY, 1000000u)) {
        ael_mailbox_fail(0xE301u, RCC_CSR);
        while (1) {}
    }

    RCC_BDCR &= ~(3u << 8);
    RCC_BDCR |= RCC_BDCR_RTCSEL_LSI;
    RCC_BDCR |= RCC_BDCR_RTCEN;

    RTC_WPR = 0xCAu;
    RTC_WPR = 0x53u;

    RTC_ISR |= RTC_ISR_INIT;
    if (!wait_until_set((volatile uint32_t *)&RTC_ISR, RTC_ISR_INITF, 1000000u)) {
        ael_mailbox_fail(0xE302u, RTC_ISR);
        while (1) {}
    }

    RTC_PRER = (127u << 16) | 249u;
    RTC_CR = 0u;
    RTC_TR = 0u;
    RTC_DR = (1u << 13) | (1u << 8) | 1u;
    RTC_ISR &= ~RTC_ISR_INIT;
    RTC_ISR &= ~RTC_ISR_RSF;
    if (!wait_until_set((volatile uint32_t *)&RTC_ISR, RTC_ISR_RSF, 1000000u)) {
        ael_mailbox_fail(0xE303u, RTC_ISR);
        while (1) {}
    }

    RTC_CR &= ~RTC_CR_ALRAE;
    if (!wait_until_set((volatile uint32_t *)&RTC_ISR, RTC_ISR_ALRAWF, 1000000u)) {
        ael_mailbox_fail(0xE304u, RTC_ISR);
        while (1) {}
    }

    RTC_ISR &= ~RTC_ISR_ALRAF;
    RTC_ALRMAR = (1u << 31) | (1u << 23) | (1u << 15) | 2u;
    RTC_CR |= RTC_CR_ALRAIE;
    RTC_CR |= RTC_CR_ALRAE;
    RTC_WPR = 0xFFu;

    EXTI_PR = EXTI_LINE_RTC_ALARM;
    EXTI_IMR |= EXTI_LINE_RTC_ALARM;
    EXTI_RTSR |= EXTI_LINE_RTC_ALARM;
    NVIC_ISER = RTC_IRQn_BIT;

    AEL_MAILBOX->detail0 = 1u;

    PWR_CR &= ~PWR_CR_PDDS;
    PWR_CR |= PWR_CR_LPDS | PWR_CR_CWUF;
    SCB_SCR |= SCB_SCR_SLEEPDEEP;
    __asm__ volatile ("dsb");
    __asm__ volatile ("wfi");
    __asm__ volatile ("isb");
    SCB_SCR &= ~SCB_SCR_SLEEPDEEP;

    if (g_alarm_fired == 0u) {
        ael_mailbox_fail(0xE305u, (RTC_ISR << 16u) | (EXTI_PR & 0xFFFFu));
        while (1) {}
    }

    RTC_WPR = 0xCAu;
    RTC_WPR = 0x53u;
    RTC_CR &= ~(RTC_CR_ALRAE | RTC_CR_ALRAIE);
    RTC_ISR &= ~RTC_ISR_ALRAF;
    RTC_WPR = 0xFFu;
    EXTI_PR = EXTI_LINE_RTC_ALARM;

    AEL_MAILBOX->detail0 = RTC_TR;
    ael_mailbox_pass();
    while (1) {}
}
