/* ch32v203_dma_mem2mem — HAL DMA1 Ch3 M2M: copy 32 words, verify
 * Based on EVT/EXAM/DMA/DMA_MEM2MEM
 */
#define AEL_MAILBOX_ADDR 0x20000600u
#include "ael_mailbox.h"
#include "ch32v20x.h"

#define BUF_SIZE 32
static uint32_t src[BUF_SIZE] = {
    0x01020304, 0x05060708, 0x090A0B0C, 0x0D0E0F10,
    0x11121314, 0x15161718, 0x191A1B1C, 0x1D1E1F20,
    0x21222324, 0x25262728, 0x292A2B2C, 0x2D2E2F30,
    0x31323334, 0x35363738, 0x393A3B3C, 0x3D3E3F40,
    0x41424344, 0x45464748, 0x494A4B4C, 0x4D4E4F50,
    0x51525354, 0x55565758, 0x595A5B5C, 0x5D5E5F60,
    0x61626364, 0x65666768, 0x696A6B6C, 0x6D6E6F70,
    0x71727374, 0x75767778, 0x797A7B7C, 0x7D7E7F80
};
static uint32_t dst[BUF_SIZE];

int main(void)
{
    DMA_InitTypeDef DMA_InitStructure = {0};
    ael_mailbox_init();

    RCC_AHBPeriphClockCmd(RCC_AHBPeriph_DMA1, ENABLE);

    DMA_StructInit(&DMA_InitStructure);
    DMA_InitStructure.DMA_PeripheralBaseAddr = (uint32_t)src;
    DMA_InitStructure.DMA_MemoryBaseAddr     = (uint32_t)dst;
    DMA_InitStructure.DMA_DIR                = DMA_DIR_PeripheralSRC;
    DMA_InitStructure.DMA_BufferSize         = BUF_SIZE * 4;   /* 128 bytes */
    DMA_InitStructure.DMA_PeripheralInc      = DMA_PeripheralInc_Enable;
    DMA_InitStructure.DMA_MemoryInc          = DMA_MemoryInc_Enable;
    DMA_InitStructure.DMA_PeripheralDataSize = DMA_PeripheralDataSize_Byte;
    DMA_InitStructure.DMA_MemoryDataSize     = DMA_MemoryDataSize_Byte;
    DMA_InitStructure.DMA_Mode               = DMA_Mode_Normal;
    DMA_InitStructure.DMA_Priority           = DMA_Priority_VeryHigh;
    DMA_InitStructure.DMA_M2M                = DMA_M2M_Enable;
    DMA_Init(DMA1_Channel3, &DMA_InitStructure);
    DMA_ClearFlag(DMA1_FLAG_TC3);
    DMA_Cmd(DMA1_Channel3, ENABLE);

    uint32_t t = 500000;
    while (DMA_GetFlagStatus(DMA1_FLAG_TC3) == RESET && --t);
    if (!t) { ael_mailbox_fail(1, 0); while (1); }

    for (int i = 0; i < BUF_SIZE; i++) {
        if (dst[i] != src[i]) { ael_mailbox_fail(2, (uint32_t)i); while (1); }
    }

    ael_mailbox_pass();
    uint32_t tick = 0;
    while (1) {
        AEL_MAILBOX->detail0 = ++tick;
        for (volatile int i = 0; i < 720000; i++);
    }
}
