/* ch32v305_uart_loopback — HAL USART1 PA9(TX)↔PA10(RX) loopback
 * Board: CH32V305RBT6
 * Wiring: PA9 ↔ PA10 jumper
 * Clock: 96 MHz
 */
#define AEL_MAILBOX_ADDR 0x20000600u
#include "ael_mailbox.h"
#include "ch32v30x.h"

static void USART1_Init(void)
{
    GPIO_InitTypeDef  GPIO_InitStructure  = {0};
    USART_InitTypeDef USART_InitStructure = {0};

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_USART1, ENABLE);

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
}

int main(void)
{
    ael_mailbox_init();
    USART1_Init();

    uint8_t pattern[] = {0xA5, 0x5A, 0x01, 0xFF};
    uint32_t err = 0;

    for (int i = 0; i < 4; i++) {
        USART_SendData(USART1, pattern[i]);
        while (USART_GetFlagStatus(USART1, USART_FLAG_TXE) == RESET);

        uint32_t t = 200000;
        while (USART_GetFlagStatus(USART1, USART_FLAG_RXNE) == RESET && --t);
        if (!t || (USART_ReceiveData(USART1) & 0xFF) != pattern[i]) {
            err |= (1u << i);
        }
    }

    if (err) { ael_mailbox_fail(err, 0); }
    else     { ael_mailbox_pass(); }

    uint32_t tick = 0;
    while (1) {
        AEL_MAILBOX->detail0 = ++tick;
        for (volatile int i = 0; i < 960000; i++);
    }
}
