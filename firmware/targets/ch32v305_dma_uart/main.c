/* ch32v305_dma_uart — Dual DMA: DMA1 Ch4→USART1_TX + DMA1 Ch5→USART1_RX loopback
 * Board: CH32V305RBT6
 * Wiring: PA9 ↔ PA10 jumper
 * Clock: 96 MHz
 *
 * TX: DMA1 Ch4 = USART1_TX (memory→peripheral)
 * RX: DMA1 Ch5 = USART1_RX (peripheral→memory)
 * Start RX DMA first, then TX DMA. Compare rx_buf to tx_buf after both TC.
 */
#define AEL_MAILBOX_ADDR 0x20000600u
#include "ael_mailbox.h"
#include "ch32v30x.h"

#define BUF_SIZE 8
static const uint8_t tx_buf[BUF_SIZE] = {0xA5, 0x5A, 0x01, 0xFF, 0x12, 0x34, 0x56, 0x78};
static uint8_t rx_buf[BUF_SIZE];

int main(void)
{
    GPIO_InitTypeDef  GPIO_InitStructure  = {0};
    USART_InitTypeDef USART_InitStructure = {0};
    DMA_InitTypeDef   DMA_InitStructure   = {0};

    ael_mailbox_init();

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_USART1, ENABLE);
    RCC_AHBPeriphClockCmd(RCC_AHBPeriph_DMA1, ENABLE);

    /* PA9: USART1 TX — AF push-pull */
    GPIO_InitStructure.GPIO_Pin   = GPIO_Pin_9;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_AF_PP;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    /* PA10: USART1 RX — input floating */
    GPIO_InitStructure.GPIO_Pin  = GPIO_Pin_10;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    USART_InitStructure.USART_BaudRate            = 115200;
    USART_InitStructure.USART_WordLength          = USART_WordLength_8b;
    USART_InitStructure.USART_StopBits            = USART_StopBits_1;
    USART_InitStructure.USART_Parity              = USART_Parity_No;
    USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    USART_InitStructure.USART_Mode                = USART_Mode_Tx | USART_Mode_Rx;
    USART_Init(USART1, &USART_InitStructure);
    USART_Cmd(USART1, ENABLE);

    /* DMA1 Ch5: USART1_RX → rx_buf (peripheral→memory) */
    DMA_DeInit(DMA1_Channel5);
    DMA_InitStructure.DMA_PeripheralBaseAddr = (uint32_t)&USART1->DATAR;
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
    DMA_Init(DMA1_Channel5, &DMA_InitStructure);

    /* DMA1 Ch4: tx_buf → USART1_TX (memory→peripheral) */
    DMA_DeInit(DMA1_Channel4);
    DMA_InitStructure.DMA_PeripheralBaseAddr = (uint32_t)&USART1->DATAR;
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
    DMA_Init(DMA1_Channel4, &DMA_InitStructure);

    /* Enable USART DMA requests; start RX first then TX */
    USART_DMACmd(USART1, USART_DMAReq_Rx, ENABLE);
    DMA_Cmd(DMA1_Channel5, ENABLE);

    USART_DMACmd(USART1, USART_DMAReq_Tx, ENABLE);
    DMA_Cmd(DMA1_Channel4, ENABLE);

    /* Wait for both TX and RX DMA to complete */
    uint32_t t = 1000000;
    while ((DMA_GetFlagStatus(DMA1_FLAG_TC4) == RESET ||
            DMA_GetFlagStatus(DMA1_FLAG_TC5) == RESET) && --t);
    if (!t) {
        uint32_t flags = (DMA_GetFlagStatus(DMA1_FLAG_TC4) ? 0 : 1) |
                         (DMA_GetFlagStatus(DMA1_FLAG_TC5) ? 0 : 2);
        ael_mailbox_fail(1, flags);
        while (1);
    }

    DMA_Cmd(DMA1_Channel4, DISABLE);
    DMA_Cmd(DMA1_Channel5, DISABLE);

    /* Compare */
    uint32_t err = 0;
    for (int i = 0; i < BUF_SIZE; i++) {
        if (rx_buf[i] != tx_buf[i]) err |= (1u << i);
    }

    if (err) { ael_mailbox_fail(2, err); }
    else     { ael_mailbox_pass(); }

    uint32_t tick = 0;
    while (1) {
        AEL_MAILBOX->detail0 = ++tick;
        for (volatile int i = 0; i < 960000; i++);
    }
}
