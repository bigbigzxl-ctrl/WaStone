/* ch32v305_can_loopback — CAN1 LoopBack self-test (no external transceiver)
 *
 * GPIO remap: GPIO_Remap1_CAN1 → PB8=CAN_RX, PB9=CAN_TX
 * Mode: CAN_Mode_LoopBack
 * Baud: ~250 kbps @ APB1=48MHz: tsjw=1, tbs1=6, tbs2=5, brp=16
 */
#define AEL_MAILBOX_ADDR 0x20000600u
#include "ael_mailbox.h"
#include "ch32v30x.h"

static void CAN1_LoopBack_Init(void)
{
    GPIO_InitTypeDef      GPIO_InitStructure    = {0};
    CAN_InitTypeDef       CAN_InitStructure     = {0};
    CAN_FilterInitTypeDef CAN_FilterInitStructure = {0};

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_AFIO | RCC_APB2Periph_GPIOB, ENABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_CAN1, ENABLE);

    GPIO_PinRemapConfig(GPIO_Remap1_CAN1, ENABLE);  /* PB8=CAN1_RX, PB9=CAN1_TX */

    GPIO_InitStructure.GPIO_Pin   = GPIO_Pin_9;
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_AF_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOB, &GPIO_InitStructure);

    GPIO_InitStructure.GPIO_Pin  = GPIO_Pin_8;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
    GPIO_Init(GPIOB, &GPIO_InitStructure);

    CAN_InitStructure.CAN_TTCM      = DISABLE;
    CAN_InitStructure.CAN_ABOM      = DISABLE;
    CAN_InitStructure.CAN_AWUM      = DISABLE;
    CAN_InitStructure.CAN_NART      = ENABLE;
    CAN_InitStructure.CAN_RFLM      = DISABLE;
    CAN_InitStructure.CAN_TXFP      = DISABLE;
    CAN_InitStructure.CAN_Mode      = CAN_Mode_LoopBack;
    CAN_InitStructure.CAN_SJW       = CAN_SJW_1tq;
    CAN_InitStructure.CAN_BS1       = CAN_BS1_6tq;
    CAN_InitStructure.CAN_BS2       = CAN_BS2_5tq;
    CAN_InitStructure.CAN_Prescaler = 16;
    CAN_Init(CAN1, &CAN_InitStructure);

    CAN_FilterInitStructure.CAN_FilterNumber         = 0;
    CAN_FilterInitStructure.CAN_FilterMode           = CAN_FilterMode_IdMask;
    CAN_FilterInitStructure.CAN_FilterScale          = CAN_FilterScale_32bit;
    CAN_FilterInitStructure.CAN_FilterIdHigh         = 0x0000;
    CAN_FilterInitStructure.CAN_FilterIdLow          = 0x0000;
    CAN_FilterInitStructure.CAN_FilterMaskIdHigh     = 0x0000;
    CAN_FilterInitStructure.CAN_FilterMaskIdLow      = 0x0000;
    CAN_FilterInitStructure.CAN_FilterFIFOAssignment = CAN_Filter_FIFO0;
    CAN_FilterInitStructure.CAN_FilterActivation     = ENABLE;
    CAN_FilterInit(&CAN_FilterInitStructure);
}

int main(void)
{
    ael_mailbox_init();
    CAN1_LoopBack_Init();

    CanTxMsg txMsg = {0};
    CanRxMsg rxMsg = {0};

    txMsg.StdId   = 0x305;
    txMsg.IDE     = CAN_Id_Standard;
    txMsg.RTR     = CAN_RTR_Data;
    txMsg.DLC     = 8;
    txMsg.Data[0] = 0xA5; txMsg.Data[1] = 0x5A;
    txMsg.Data[2] = 0x03; txMsg.Data[3] = 0x05;
    txMsg.Data[4] = 0x12; txMsg.Data[5] = 0x34;
    txMsg.Data[6] = 0x56; txMsg.Data[7] = 0x78;

    uint8_t mbox = CAN_Transmit(CAN1, &txMsg);

    uint32_t t = 200000;
    while ((CAN_TransmitStatus(CAN1, mbox) != CAN_TxStatus_Ok) && --t);
    if (!t) { ael_mailbox_fail(1, 0); while (1); }

    t = 200000;
    while (CAN_MessagePending(CAN1, CAN_FIFO0) == 0 && --t);
    if (!t) { ael_mailbox_fail(2, 0); while (1); }

    CAN_Receive(CAN1, CAN_FIFO0, &rxMsg);

    if (rxMsg.DLC != 8) { ael_mailbox_fail(3, rxMsg.DLC); while (1); }

    for (uint8_t i = 0; i < 8; i++) {
        if (rxMsg.Data[i] != txMsg.Data[i]) {
            ael_mailbox_fail(4, ((uint32_t)rxMsg.Data[i] << 8) | i);
            while (1);
        }
    }

    ael_mailbox_pass();
    uint32_t tick = 0;
    while (1) {
        AEL_MAILBOX->detail0 = ++tick;
        for (volatile int i = 0; i < 960000; i++);
    }
}
