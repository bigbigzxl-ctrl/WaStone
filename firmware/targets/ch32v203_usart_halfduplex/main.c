/* ch32v203_usart_halfduplex — USART2 + USART3 half-duplex loopback
 *
 * USART2: PA2 (AF_OD, needs external 4.7kΩ pull-up to 3V3)
 * USART3: PB10 (AF_OD, needs external 4.7kΩ pull-up to 3V3)
 * Wire: PA2 ↔ PB10 (single bidirectional wire + pull-up)
 *
 * Test sequence:
 *   1. USART3 → USART2: send TxBuf2 (4 bytes), receive into RxBuf1, compare
 *   2. USART2 → USART3: send TxBuf1 (4 bytes), receive into RxBuf2, compare
 *
 * Fail codes:
 *   1 = direction 1 (USART3→USART2): data mismatch bitmask
 *   2 = direction 2 (USART2→USART3): data mismatch bitmask
 *   3 = RX timeout (detail0 = step that timed out)
 *
 * Stage 1+: new wire PA2 ↔ PB10 + 4.7kΩ pull-up to 3V3 on that line.
 */
#define AEL_MAILBOX_ADDR 0x20000600u
#include "ael_mailbox.h"
#include "ch32v20x.h"

#define BUF_SIZE 4
static const uint8_t TxBuf1[BUF_SIZE] = {0xA5, 0x5A, 0x12, 0x34};
static const uint8_t TxBuf2[BUF_SIZE] = {0xDE, 0xAD, 0xBE, 0xEF};
static uint8_t RxBuf1[BUF_SIZE];
static uint8_t RxBuf2[BUF_SIZE];

static void usart_send_byte(USART_TypeDef *usart, uint8_t b)
{
    while (USART_GetFlagStatus(usart, USART_FLAG_TXE) == RESET);
    USART_SendData(usart, b);
    while (USART_GetFlagStatus(usart, USART_FLAG_TC) == RESET);
}

static int usart_recv_byte(USART_TypeDef *usart, uint8_t *out)
{
    uint32_t t = 500000;
    while (USART_GetFlagStatus(usart, USART_FLAG_RXNE) == RESET && --t);
    if (!t) return -1;
    *out = (uint8_t)USART_ReceiveData(usart);
    return 0;
}

int main(void)
{
    GPIO_InitTypeDef  GPIO_InitStructure  = {0};
    USART_InitTypeDef USART_InitStructure = {0};

    ael_mailbox_init();

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_GPIOB, ENABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART2 | RCC_APB1Periph_USART3, ENABLE);

    /* PA2: USART2 TX (half-duplex, AF_OD — needs pull-up) */
    GPIO_InitStructure.GPIO_Pin   = GPIO_Pin_2;
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_AF_OD;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    /* PB10: USART3 TX (half-duplex, AF_OD — needs pull-up) */
    GPIO_InitStructure.GPIO_Pin   = GPIO_Pin_10;
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_AF_OD;
    GPIO_Init(GPIOB, &GPIO_InitStructure);

    USART_InitStructure.USART_BaudRate            = 115200;
    USART_InitStructure.USART_WordLength          = USART_WordLength_8b;
    USART_InitStructure.USART_StopBits            = USART_StopBits_1;
    USART_InitStructure.USART_Parity              = USART_Parity_No;
    USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    USART_InitStructure.USART_Mode                = USART_Mode_Tx | USART_Mode_Rx;

    USART_Init(USART2, &USART_InitStructure);
    USART_Init(USART3, &USART_InitStructure);

    USART_HalfDuplexCmd(USART2, ENABLE);
    USART_HalfDuplexCmd(USART3, ENABLE);

    USART_Cmd(USART2, ENABLE);
    USART_Cmd(USART3, ENABLE);

    /* Direction 1: USART3 TX → USART2 RX */
    for (int i = 0; i < BUF_SIZE; i++) {
        usart_send_byte(USART3, TxBuf2[i]);
        if (usart_recv_byte(USART2, &RxBuf1[i]) < 0) {
            ael_mailbox_fail(3, 1);
            while (1);
        }
    }

    uint32_t err1 = 0;
    for (int i = 0; i < BUF_SIZE; i++) {
        if (RxBuf1[i] != TxBuf2[i]) err1 |= (1u << i);
    }
    if (err1) { ael_mailbox_fail(1, err1); while (1); }

    /* Direction 2: USART2 TX → USART3 RX */
    for (int i = 0; i < BUF_SIZE; i++) {
        usart_send_byte(USART2, TxBuf1[i]);
        if (usart_recv_byte(USART3, &RxBuf2[i]) < 0) {
            ael_mailbox_fail(3, 2);
            while (1);
        }
    }

    uint32_t err2 = 0;
    for (int i = 0; i < BUF_SIZE; i++) {
        if (RxBuf2[i] != TxBuf1[i]) err2 |= (1u << i);
    }
    if (err2) { ael_mailbox_fail(2, err2); while (1); }

    ael_mailbox_pass();
    AEL_MAILBOX->detail0 = 0;

    uint32_t tick = 0;
    while (1) {
        AEL_MAILBOX->detail0 = ++tick;
        for (volatile int i = 0; i < 720000; i++);
    }
}
