#include <stdint.h>

#define AEL_MAILBOX_ADDR 0x20002400u
#include "../stm32f103c6/ael_mailbox.h"

#define SCB_CPUID       (*(volatile uint32_t *)0xE000ED00u)
#define DBGMCU_IDCODE   (*(volatile uint32_t *)0xE0042000u)
#define FLASH_SIZE_KB   (*(volatile uint16_t *)0x1FFFF7E0u)
#define UID_W0          (*(volatile uint32_t *)0x1FFFF7E8u)
#define UID_W1          (*(volatile uint32_t *)0x1FFFF7ECu)
#define UID_W2          (*(volatile uint32_t *)0x1FFFF7F0u)

#define RCC_BASE        0x40021000u
#define RCC_CR          (*(volatile uint32_t *)(RCC_BASE + 0x00u))
#define RCC_CFGR        (*(volatile uint32_t *)(RCC_BASE + 0x04u))

#define ERR_CPUID       0x31u
#define ERR_DEVID       0x32u
#define ERR_FLASH       0x33u
#define ERR_UID         0x34u
#define ERR_CLOCK       0x35u

int main(void) {
    uint32_t cpuid = SCB_CPUID;
    uint32_t idcode = DBGMCU_IDCODE;
    uint32_t dev_id = idcode & 0x0FFFu;
    uint32_t flash_kb = FLASH_SIZE_KB;
    uint32_t uid0 = UID_W0;
    uint32_t uid1 = UID_W1;
    uint32_t uid2 = UID_W2;
    uint32_t uid_mix = uid0 ^ uid1 ^ uid2;
    uint32_t hsi_ready = (RCC_CR >> 1u) & 1u;
    uint32_t sws = (RCC_CFGR >> 2u) & 0x3u;

    ael_mailbox_init();

    if (cpuid != 0x411FC231u) {
        ael_mailbox_fail(ERR_CPUID, cpuid);
        while (1) {}
    }

    if (dev_id != 0x412u) {
        ael_mailbox_fail(ERR_DEVID, idcode);
        while (1) {}
    }

    if (flash_kb < 32u || flash_kb > 128u) {
        ael_mailbox_fail(ERR_FLASH, flash_kb);
        while (1) {}
    }

    if (uid_mix == 0u) {
        ael_mailbox_fail(ERR_UID, uid0 | uid1 | uid2);
        while (1) {}
    }

    if (hsi_ready == 0u || sws > 0x2u) {
        ael_mailbox_fail(ERR_CLOCK, (hsi_ready << 16u) | sws);
        while (1) {}
    }

    AEL_MAILBOX->detail0 = (flash_kb << 16u) | dev_id;
    ael_mailbox_pass();

    while (1) {}
}
