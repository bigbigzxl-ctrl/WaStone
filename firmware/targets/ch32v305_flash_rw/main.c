/* ch32v305_flash_rw — FLASH_ROM_ERASE + FLASH_ROM_WRITE at 0x08008000
 * CH32V30x fast API: StartAddr and Length must be 256-byte aligned.
 * Test block at 32KB offset — safe from firmware.
 * Erased state on CH32V30x reads as 0xe339 (not 0xFFFF).
 */
#define AEL_MAILBOX_ADDR 0x20000600u
#include "ael_mailbox.h"
#include "ch32v30x.h"

#define TEST_ADDR  0x08008000u   /* 32KB into flash, 256-byte aligned */
#define TEST_LEN   256u          /* minimum erase/write unit */
#define TEST_WORD  0xBEEF5AA5u

static uint32_t wbuf[TEST_LEN / 4];

int main(void)
{
    FLASH_Status status;
    ael_mailbox_init();

    FLASH_Unlock();

    /* Erase 256 bytes */
    status = FLASH_ROM_ERASE(TEST_ADDR, TEST_LEN);
    if (status != FLASH_COMPLETE) {
        ael_mailbox_fail(1, (uint32_t)status);
        while (1);
    }

    /* Fill write buffer with test pattern */
    for (uint32_t i = 0; i < TEST_LEN / 4; i++)
        wbuf[i] = TEST_WORD;

    /* Write 256 bytes */
    status = FLASH_ROM_WRITE(TEST_ADDR, wbuf, TEST_LEN);
    if (status != FLASH_COMPLETE) {
        ael_mailbox_fail(2, (uint32_t)status);
        while (1);
    }

    FLASH_Lock();

    /* Verify first word */
    uint32_t readback = *(volatile uint32_t *)TEST_ADDR;
    if (readback != TEST_WORD) {
        ael_mailbox_fail(3, readback);
    } else {
        ael_mailbox_pass();
    }

    uint32_t tick = 0;
    while (1) {
        AEL_MAILBOX->detail0 = ++tick;
        for (volatile int i = 0; i < 960000; i++);
    }
}
