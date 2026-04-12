/* ch32v305_rtc_self — HAL RTC with LSI (~40kHz): counter must advance in 2s
 * Uses LSI (internal oscillator) — no external crystal needed
 */
#define AEL_MAILBOX_ADDR 0x20000600u
#include "ael_mailbox.h"
#include "ch32v30x.h"

static void delay_ms(uint32_t ms)
{
    for (uint32_t i = 0; i < ms * 9600; i++) __asm__("nop");
}

int main(void)
{
    ael_mailbox_init();

    RCC_APB1PeriphClockCmd(RCC_APB1Periph_PWR | RCC_APB1Periph_BKP, ENABLE);
    PWR_BackupAccessCmd(ENABLE);
    BKP_DeInit();

    RCC_LSICmd(ENABLE);
    uint32_t t = 500000;
    while (RCC_GetFlagStatus(RCC_FLAG_LSIRDY) == RESET && --t);
    if (!t) { ael_mailbox_fail(1, 0); while (1); }

    RCC_RTCCLKConfig(RCC_RTCCLKSource_LSI);
    RCC_RTCCLKCmd(ENABLE);

    RTC_WaitForSynchro();
    RTC_WaitForLastTask();

    RTC_SetPrescaler(39999);
    RTC_WaitForLastTask();

    RTC_SetCounter(0);
    RTC_WaitForLastTask();

    RTC_WaitForSynchro();
    uint32_t c0 = RTC_GetCounter();

    delay_ms(2000);

    RTC_WaitForSynchro();
    uint32_t c1 = RTC_GetCounter();

    if (c1 > c0) {
        ael_mailbox_pass();
    } else {
        ael_mailbox_fail(2, c1);
    }

    uint32_t tick = 0;
    while (1) {
        AEL_MAILBOX->detail0 = ++tick;
        delay_ms(500);
    }
}
