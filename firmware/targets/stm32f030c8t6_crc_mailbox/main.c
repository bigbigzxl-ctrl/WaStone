#include <stdint.h>

#define AEL_MAILBOX_ADDR 0x20001C00u
#include "../stm32f030c8t6/ael_mailbox.h"

#define RCC_BASE        0x40021000u
#define RCC_AHBENR      (*(volatile uint32_t *)(RCC_BASE + 0x14u))
#define RCC_AHBENR_CRCEN (1u << 6)

#define CRC_BASE        0x40023000u
#define CRC_DR          (*(volatile uint32_t *)(CRC_BASE + 0x00u))
#define CRC_CR          (*(volatile uint32_t *)(CRC_BASE + 0x08u))
#define CRC_CR_RESET    (1u << 0)

static const uint32_t k_test_words[8] = {
    0x12345678u, 0xDEADBEEFu, 0xCAFEBABEu, 0xA5A5A5A5u,
    0x00FF00FFu, 0xFFFFFFFFu, 0x01234567u, 0x89ABCDEFu,
};

static uint32_t sw_crc32_mpeg_word(uint32_t crc, uint32_t word)
{
    for (int bit = 31; bit >= 0; --bit) {
        uint32_t in = (word >> (uint32_t)bit) & 1u;
        if (((crc >> 31u) & 1u) ^ in) {
            crc = (crc << 1u) ^ 0x04C11DB7u;
        } else {
            crc <<= 1u;
        }
    }
    return crc;
}

static uint32_t hw_crc_run(void)
{
    CRC_CR = CRC_CR_RESET;
    for (uint32_t i = 0u; i < 8u; ++i) {
        CRC_DR = k_test_words[i];
    }
    return CRC_DR;
}

int main(void)
{
    ael_mailbox_init();

    RCC_AHBENR |= RCC_AHBENR_CRCEN;
    (void)RCC_AHBENR;

    uint32_t sw = 0xFFFFFFFFu;
    for (uint32_t i = 0u; i < 8u; ++i) {
        sw = sw_crc32_mpeg_word(sw, k_test_words[i]);
    }

    uint32_t hw1 = hw_crc_run();
    if (hw1 == 0xFFFFFFFFu) {
        ael_mailbox_fail(0xE2C1u, hw1);
        while (1) {}
    }
    if (hw1 != sw) {
        ael_mailbox_fail(0xE2C2u, hw1);
        while (1) {}
    }

    uint32_t hw2 = hw_crc_run();
    if (hw2 != hw1) {
        ael_mailbox_fail(0xE2C3u, hw2);
        while (1) {}
    }

    AEL_MAILBOX->detail0 = hw1;
    ael_mailbox_pass();
    while (1) {}
}
