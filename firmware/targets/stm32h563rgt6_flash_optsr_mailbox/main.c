/*
 * stm32h563rgt6_flash_optsr_mailbox — Flash option bytes read self-test
 *
 * Reads the FLASH option status registers (OPTSR_CUR and OPTSR2_CUR) and
 * verifies the device is not in the "Closed" (0x8F) product state.
 *
 * On STM32H563, the traditional FLASH_OPTR is replaced by OPTSR_CUR:
 *   OPTSR_CUR  = FLASH_R_BASE + 0x050  (current option status register)
 *   OPTSR2_CUR = FLASH_R_BASE + 0x070  (current option status register 2)
 *
 * OPTSR_CUR layout:
 *   bits[7:0]   = BOR_LEV / options
 *   bits[15:8]  = PRODUCT_STATE
 *     0x17 = Open (RDP level 0 equivalent)
 *     0x51 = Provisioned
 *     0x6F = iRoT Provisioned
 *     0x8F = Closed (RDP level 2 — flash permanently locked)
 *     0xB4 = Locked (RDP level 1)
 *   bit16       = IO_VDDIO2_HSLV
 *   bit17       = IO_VDDA_HSLV
 *   bit24       = SWAP_BANK
 *   bit25       = SRAM1_3RST
 *   bit27       = SRAM2RST
 *
 * OPTSR2_CUR layout:
 *   bits[31:24] = TZEN (0xB4 = TrustZone enabled)
 *
 * FLASH_R_BASE = 0x40022000
 *
 * FAIL codes:
 *   0xE001 — OPTSR_CUR = 0 (Flash controller not accessible)
 *   0xE002 — PRODUCT_STATE = 0x8F (device is Closed — permanently locked)
 *
 * detail0: [31:16]=OPTSR2_CUR[31:16], [15:0]=OPTSR_CUR[15:0]
 */

#include <stdint.h>
#include "../ael_mailbox.h"

#define FLASH_R_BASE    0x40022000u
#define FLASH_OPTSR_CUR  (*(volatile uint32_t *)(FLASH_R_BASE + 0x050u))
#define FLASH_OPTSR2_CUR (*(volatile uint32_t *)(FLASH_R_BASE + 0x070u))

#define PRODUCT_STATE_CLOSED  0x8Fu

int main(void)
{
    ael_mailbox_init();

    /* Read current option status registers */
    uint32_t optsr  = FLASH_OPTSR_CUR;
    uint32_t optsr2 = FLASH_OPTSR2_CUR;

    /* Verify Flash controller is accessible */
    if (optsr == 0u) {
        ael_mailbox_fail(0xE001u, optsr);
        while (1) {}
    }

    /* Extract PRODUCT_STATE field at bits[15:8] */
    uint32_t product_state = (optsr >> 8) & 0xFFu;

    /* Fail if device is Closed (permanently locked) */
    if (product_state == PRODUCT_STATE_CLOSED) {
        ael_mailbox_fail(0xE002u, optsr);
        while (1) {}
    }

    /* detail0: upper half = OPTSR2[31:16], lower half = OPTSR[15:0] */
    AEL_MAILBOX->detail0 = ((optsr2 & 0xFFFF0000u)) | (optsr & 0xFFFFu);
    ael_mailbox_pass();
    while (1) {}
    return 0;
}
