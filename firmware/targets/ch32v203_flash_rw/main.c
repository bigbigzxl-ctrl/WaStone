/* ch32v203_flash_rw — HAL FLASH: erase page at 0x08008000, program, verify
 * Based on EVT/EXAM/FLASH/FLASH_Program
 * Page size = 4096 bytes on CH32V20x; uses page at 32KB offset (safe from FW)
 */
#define AEL_MAILBOX_ADDR 0x20000600u
#include "ael_mailbox.h"
#include "ch32v20x.h"

#define TEST_ADDR  0x08008000u   /* 32KB into flash — safe from firmware */
#define TEST_HW0   0x5AA5u
#define TEST_HW1   0xBEEFu
#define TEST_WORD  0xBEEF5AA5u   /* HW0 at low addr, HW1 at high addr */

int main(void)
{
    FLASH_Status status;
    ael_mailbox_init();

    FLASH_Unlock();
    FLASH_ClearFlag(FLASH_FLAG_BSY | FLASH_FLAG_EOP | FLASH_FLAG_WRPRTERR);

    /* Erase page (4KB) at TEST_ADDR */
    status = FLASH_ErasePage(TEST_ADDR);
    if (status != FLASH_COMPLETE) {
        ael_mailbox_fail(1, (uint32_t)status);
        while (1);
    }

    /* Program two half-words = one 32-bit word */
    status = FLASH_ProgramHalfWord(TEST_ADDR,     TEST_HW0);
    if (status != FLASH_COMPLETE) { ael_mailbox_fail(2, (uint32_t)status); while (1); }

    status = FLASH_ProgramHalfWord(TEST_ADDR + 2, TEST_HW1);
    if (status != FLASH_COMPLETE) { ael_mailbox_fail(3, (uint32_t)status); while (1); }

    FLASH_Lock();

    /* Verify read-back */
    uint32_t readback = *(volatile uint32_t *)TEST_ADDR;
    if (readback != TEST_WORD) {
        ael_mailbox_fail(4, readback);
    } else {
        ael_mailbox_pass();
    }

    uint32_t tick = 0;
    while (1) {
        AEL_MAILBOX->detail0 = ++tick;
        for (volatile int i = 0; i < 720000; i++);
    }
}
