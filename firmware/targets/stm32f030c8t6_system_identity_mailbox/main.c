#include <stdint.h>

#define AEL_MAILBOX_ADDR 0x20001C00u
#include "../stm32f030c8t6/ael_mailbox.h"

#define SCB_CPUID       (*(volatile uint32_t *)0xE000ED00u)
#define DBGMCU_IDCODE   (*(volatile uint32_t *)0x40015800u)
#define FLASH_SIZE_KB   (*(volatile uint16_t *)0x1FFFF7CCu)
#define UID_W0          (*(volatile uint32_t *)0x1FFFF7ACu)
#define UID_W1          (*(volatile uint32_t *)0x1FFFF7B0u)
#define UID_W2          (*(volatile uint32_t *)0x1FFFF7B4u)

#define RCC_BASE        0x40021000u
#define RCC_CR          (*(volatile uint32_t *)(RCC_BASE + 0x00u))
#define RCC_CFGR        (*(volatile uint32_t *)(RCC_BASE + 0x04u))

int main(void)
{
    uint32_t cpuid = SCB_CPUID;
    uint32_t idcode = DBGMCU_IDCODE;
    uint32_t dev_id = idcode & 0x0FFFu;
    uint32_t flash_kb = FLASH_SIZE_KB;
    uint32_t uid_mix = UID_W0 ^ UID_W1 ^ UID_W2;
    uint32_t hsi_ready = (RCC_CR >> 1u) & 1u;
    uint32_t sws = (RCC_CFGR >> 2u) & 0x3u;

    ael_mailbox_init();

    if (cpuid != 0x410CC200u) {
        ael_mailbox_fail(0xE201u, cpuid);
        while (1) {}
    }
    if (dev_id != 0x440u) {
        ael_mailbox_fail(0xE202u, idcode);
        while (1) {}
    }
    if (flash_kb < 32u || flash_kb > 128u) {
        ael_mailbox_fail(0xE203u, flash_kb);
        while (1) {}
    }
    if (uid_mix == 0u) {
        ael_mailbox_fail(0xE204u, uid_mix);
        while (1) {}
    }
    if (hsi_ready == 0u || sws > 0x2u) {
        ael_mailbox_fail(0xE205u, (hsi_ready << 16u) | sws);
        while (1) {}
    }

    AEL_MAILBOX->detail0 = (flash_kb << 16u) | dev_id;
    ael_mailbox_pass();
    while (1) {}
}
