/* ch32v203_spi_dma — SPI1 master loopback via DMA
 *
 * SPI1 master, NSS_Soft, 8-bit, CPOL_Low/CPHA_1Edge, BRP_64 (~562.5 kHz at 36MHz APB2).
 * PA5=SCK(AF_PP), PA6=MISO(IN_FLOAT), PA7=MOSI(AF_PP).
 * DMA1_Ch2 = SPI1_RX (peripheral→memory)
 * DMA1_Ch3 = SPI1_TX (memory→peripheral)
 * Start RX first, then TX. Wait both TC flags. Compare rx_buf to tx_buf.
 *
 * Stage 1 wiring: PA7(MOSI) ↔ PA6(MISO) — already present from SPI loopback.
 * detail0 on PASS: 0.
 */
#define AEL_MAILBOX_ADDR 0x20000600u
#include "ael_mailbox.h"
#include "ch32v20x.h"

#define BUF_SIZE 8
static const uint8_t tx_buf[BUF_SIZE] = {0xA5, 0x5A, 0x01, 0xFF, 0x12, 0x34, 0x56, 0x78};
static uint8_t rx_buf[BUF_SIZE];

int main(void)
{
    GPIO_InitTypeDef GPIO_InitStructure = {0};
    SPI_InitTypeDef  SPI_InitStructure  = {0};
    DMA_InitTypeDef  DMA_InitStructure  = {0};

    ael_mailbox_init();

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_SPI1, ENABLE);
    RCC_AHBPeriphClockCmd(RCC_AHBPeriph_DMA1, ENABLE);

    /* PA5: SCK, PA7: MOSI — AF push-pull */
    GPIO_InitStructure.GPIO_Pin   = GPIO_Pin_5 | GPIO_Pin_7;
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_AF_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    /* PA6: MISO — input floating */
    GPIO_InitStructure.GPIO_Pin  = GPIO_Pin_6;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    /* SPI1: master, 8-bit */
    SPI_InitStructure.SPI_Direction         = SPI_Direction_2Lines_FullDuplex;
    SPI_InitStructure.SPI_Mode              = SPI_Mode_Master;
    SPI_InitStructure.SPI_DataSize          = SPI_DataSize_8b;
    SPI_InitStructure.SPI_CPOL              = SPI_CPOL_Low;
    SPI_InitStructure.SPI_CPHA              = SPI_CPHA_1Edge;
    SPI_InitStructure.SPI_NSS               = SPI_NSS_Soft;
    SPI_InitStructure.SPI_BaudRatePrescaler = SPI_BaudRatePrescaler_64;
    SPI_InitStructure.SPI_FirstBit          = SPI_FirstBit_MSB;
    SPI_InitStructure.SPI_CRCPolynomial     = 7;
    SPI_Init(SPI1, &SPI_InitStructure);

    /* DMA1 Ch2: SPI1_RX → rx_buf */
    DMA_DeInit(DMA1_Channel2);
    DMA_InitStructure.DMA_PeripheralBaseAddr = (uint32_t)&SPI1->DATAR;
    DMA_InitStructure.DMA_MemoryBaseAddr     = (uint32_t)rx_buf;
    DMA_InitStructure.DMA_DIR                = DMA_DIR_PeripheralSRC;
    DMA_InitStructure.DMA_BufferSize         = BUF_SIZE;
    DMA_InitStructure.DMA_PeripheralInc      = DMA_PeripheralInc_Disable;
    DMA_InitStructure.DMA_MemoryInc          = DMA_MemoryInc_Enable;
    DMA_InitStructure.DMA_PeripheralDataSize = DMA_PeripheralDataSize_Byte;
    DMA_InitStructure.DMA_MemoryDataSize     = DMA_MemoryDataSize_Byte;
    DMA_InitStructure.DMA_Mode               = DMA_Mode_Normal;
    DMA_InitStructure.DMA_Priority           = DMA_Priority_VeryHigh;
    DMA_InitStructure.DMA_M2M                = DMA_M2M_Disable;
    DMA_Init(DMA1_Channel2, &DMA_InitStructure);

    /* DMA1 Ch3: tx_buf → SPI1_TX */
    DMA_DeInit(DMA1_Channel3);
    DMA_InitStructure.DMA_PeripheralBaseAddr = (uint32_t)&SPI1->DATAR;
    DMA_InitStructure.DMA_MemoryBaseAddr     = (uint32_t)tx_buf;
    DMA_InitStructure.DMA_DIR                = DMA_DIR_PeripheralDST;
    DMA_InitStructure.DMA_BufferSize         = BUF_SIZE;
    DMA_InitStructure.DMA_PeripheralInc      = DMA_PeripheralInc_Disable;
    DMA_InitStructure.DMA_MemoryInc          = DMA_MemoryInc_Enable;
    DMA_InitStructure.DMA_PeripheralDataSize = DMA_PeripheralDataSize_Byte;
    DMA_InitStructure.DMA_MemoryDataSize     = DMA_MemoryDataSize_Byte;
    DMA_InitStructure.DMA_Mode               = DMA_Mode_Normal;
    DMA_InitStructure.DMA_Priority           = DMA_Priority_High;
    DMA_InitStructure.DMA_M2M                = DMA_M2M_Disable;
    DMA_Init(DMA1_Channel3, &DMA_InitStructure);

    /* Enable SPI DMA requests: RX first, then TX */
    SPI_I2S_DMACmd(SPI1, SPI_I2S_DMAReq_Rx, ENABLE);
    DMA_Cmd(DMA1_Channel2, ENABLE);

    SPI_Cmd(SPI1, ENABLE);

    SPI_I2S_DMACmd(SPI1, SPI_I2S_DMAReq_Tx, ENABLE);
    DMA_Cmd(DMA1_Channel3, ENABLE);

    /* Wait for both DMA transfers to complete */
    uint32_t t = 1000000;
    while ((DMA_GetFlagStatus(DMA1_FLAG_TC2) == RESET ||
            DMA_GetFlagStatus(DMA1_FLAG_TC3) == RESET) && --t);
    if (!t) {
        uint32_t flags = (DMA_GetFlagStatus(DMA1_FLAG_TC2) ? 0 : 1) |
                         (DMA_GetFlagStatus(DMA1_FLAG_TC3) ? 0 : 2);
        ael_mailbox_fail(1, flags);
        while (1);
    }

    DMA_Cmd(DMA1_Channel2, DISABLE);
    DMA_Cmd(DMA1_Channel3, DISABLE);

    /* Compare */
    uint32_t err = 0;
    for (int i = 0; i < BUF_SIZE; i++) {
        if (rx_buf[i] != tx_buf[i]) err |= (1u << i);
    }

    if (err) {
        ael_mailbox_fail(2, err);
    } else {
        ael_mailbox_pass();
        AEL_MAILBOX->detail0 = 0;
    }

    uint32_t tick = 0;
    while (1) {
        AEL_MAILBOX->detail0 = ++tick;
        for (volatile int i = 0; i < 720000; i++);
    }
}
