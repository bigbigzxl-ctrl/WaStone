/* ch32v203_usart_halfduplex — USART2 half-duplex self-loopback
 *
 * USART2 in HDSEL (single-wire half-duplex) mode on PA2 (AF_OD).
 * PA3 configured as GPIO input + internal pull-up via the existing Stage-1
 * PA2↔PA3 jumper, providing the pull-up required by open-drain PA2.
 *
 * In HDSEL mode the USART TX output is wired internally to the USART RX
 * input: transmitted bytes are immediately echoed back to the receive
 * data register (same USART, single pin, no second USART needed).
 *
 * Test sequence (all on USART2 / PA2):
 *   1. Enable TE+RE simultaneously — in self-loopback the sender
 *      does not conflict with itself.
 *   2. Transmit TxBuf (4 bytes) one at a time.
 *   3. After each TC, read back RXNE byte and compare.
 *
 * Wiring required: PA2 ↔ PA3 (Stage-1 jumper, already in place).
 *   PA3 as IPU provides ~40kΩ pull-up to PA2 through the wire.
 *
 * Fail codes:
 *   1 = data mismatch (detail0 = byte index)
 *   3 = RX timeout  (detail0[31:16]=USART2_SR, detail0[15:0]=byte index)
 */
#define AEL_MAILBOX_ADDR 0x20000600u
#include "ael_mailbox.h"
#include "ch32v20x.h"

#define BUF_SIZE 4
static const uint8_t TxBuf[BUF_SIZE] = {0xA5, 0x5A, 0x12, 0x34};

int main(void)
{
    GPIO_InitTypeDef  GPIO_InitStructure  = {0};
    USART_InitTypeDef USART_InitStructure = {0};

    ael_mailbox_init();

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_AFIO, ENABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART2, ENABLE);

    /* PA3: GPIO input + internal pull-up → acts as pull-up resistor for PA2
     * via the Stage-1 PA2↔PA3 jumper. */
    GPIO_InitStructure.GPIO_Pin  = GPIO_Pin_3;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    /* PA2: USART2 TX, AF open-drain (half-duplex single-wire) */
    GPIO_InitStructure.GPIO_Pin   = GPIO_Pin_2;
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_AF_OD;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    USART_InitStructure.USART_BaudRate            = 9600;
    USART_InitStructure.USART_WordLength          = USART_WordLength_8b;
    USART_InitStructure.USART_StopBits            = USART_StopBits_1;
    USART_InitStructure.USART_Parity              = USART_Parity_No;
    USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    USART_InitStructure.USART_Mode                = USART_Mode_Tx | USART_Mode_Rx;
    USART_Init(USART2, &USART_InitStructure);

    /* Enable HDSEL: RX is connected internally to TX pin (PA2) */
    USART_HalfDuplexCmd(USART2, ENABLE);
    USART_Cmd(USART2, ENABLE);

    /* Flush any stale RXNE */
    if (USART2->STATR & USART_FLAG_RXNE) (void)USART2->DATAR;

    for (int i = 0; i < BUF_SIZE; i++) {
        /* Send */
        while (!(USART2->STATR & USART_FLAG_TXE));
        USART2->DATAR = TxBuf[i];
        while (!(USART2->STATR & USART_FLAG_TC));

        /* Receive echo */
        uint32_t t = 720000;
        while (!(USART2->STATR & USART_FLAG_RXNE) && --t);
        if (!t) {
            uint32_t d = ((uint32_t)(USART2->STATR & 0xFFFF) << 16) | (uint32_t)i;
            ael_mailbox_fail(3, d);
            while (1) { AEL_MAILBOX->detail0 = d; }
        }
        uint8_t rx = (uint8_t)USART2->DATAR;
        if (rx != TxBuf[i]) {
            ael_mailbox_fail(1, (uint32_t)i);
            while (1);
        }
    }

    ael_mailbox_pass();
    AEL_MAILBOX->detail0 = 0x600D;

    uint32_t tick = 0;
    while (1) {
        AEL_MAILBOX->detail0 = ++tick;
        for (volatile int i = 0; i < 720000; i++);
    }
}
