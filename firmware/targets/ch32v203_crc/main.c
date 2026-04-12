/* ch32v203_crc — HAL CRC32: 32-word block → expect 0x199AC3CA
 * Based on EVT/EXAM/CRC/CRC_Calculation
 */
#define AEL_MAILBOX_ADDR 0x20000600u
#include "ael_mailbox.h"
#include "ch32v20x.h"

#define BUF_SIZE 32
static const uint32_t buf[BUF_SIZE] = {
    0x01020304, 0x05060708, 0x090A0B0C, 0x0D0E0F10,
    0x11121314, 0x15161718, 0x191A1B1C, 0x1D1E1F20,
    0x21222324, 0x25262728, 0x292A2B2C, 0x2D2E2F30,
    0x31323334, 0x35363738, 0x393A3B3C, 0x3D3E3F40,
    0x41424344, 0x45464748, 0x494A4B4C, 0x4D4E4F50,
    0x51525354, 0x55565758, 0x595A5B5C, 0x5D5E5F60,
    0x61626364, 0x65666768, 0x696A6B6C, 0x6D6E6F70,
    0x71727374, 0x75767778, 0x797A7B7C, 0x7D7E7F80
};
#define EXPECTED_CRC 0x199AC3CAu

int main(void)
{
    ael_mailbox_init();

    RCC_AHBPeriphClockCmd(RCC_AHBPeriph_CRC, ENABLE);

    uint32_t crc = CRC_CalcBlockCRC((uint32_t *)buf, BUF_SIZE);

    if (crc != EXPECTED_CRC) {
        ael_mailbox_fail(1, crc);
    } else {
        ael_mailbox_pass();
    }

    uint32_t tick = 0;
    while (1) {
        AEL_MAILBOX->detail0 = ++tick;
        for (volatile int i = 0; i < 720000; i++);
    }
}
