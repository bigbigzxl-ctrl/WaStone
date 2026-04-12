/* UART.h — minimal stub for USB CDC banner test (no real UART bridging) */
#ifndef __UART_H__
#define __UART_H__

#include <stdint.h>
#include "ch32v20x.h"

#define DEF_UARTx_RX_BUF_LEN       (4 * 512)
#define DEF_UARTx_TX_BUF_LEN       (2 * 512)
#define DEF_USB_FS_PACK_LEN         64
#define DEF_UARTx_TX_BUF_NUM_MAX    (DEF_UARTx_TX_BUF_LEN / DEF_USB_FS_PACK_LEN)
#define DEF_UARTx_BAUDRATE          115200
#define DEF_UARTx_STOPBIT           0
#define DEF_UARTx_PARITY            0
#define DEF_UARTx_DATABIT           8
#define DEF_UARTx_RX_TIMEOUT        30
#define DEF_UARTx_USB_UP_TIMEOUT    60000
#define DEF_UART2_TX_DMA_CH         DMA1_Channel7
#define DEF_UART2_RX_DMA_CH         DMA1_Channel6

typedef struct __attribute__((packed)) _UART_CTL {
    uint16_t Rx_LoadPtr;
    uint16_t Rx_DealPtr;
    volatile uint16_t Rx_RemainLen;
    uint8_t  Rx_TimeOut;
    uint8_t  Rx_TimeOutMax;
    volatile uint16_t Tx_LoadNum;
    volatile uint16_t Tx_DealNum;
    volatile uint16_t Tx_RemainNum;
    volatile uint16_t Tx_PackLen[DEF_UARTx_TX_BUF_NUM_MAX];
    uint8_t  Tx_Flag;
    uint8_t  Recv1;
    uint8_t  USB_Down_StopFlag;
    uint8_t  USB_Up_IngFlag;
    uint8_t  Com_Cfg[8];
} UART_CTL, *pUART_CTL;

extern UART_CTL Uart;
extern uint8_t  UART2_Tx_Buf[DEF_UARTx_TX_BUF_LEN];
extern uint8_t  UART2_Rx_Buf[DEF_UARTx_RX_BUF_LEN];

void UART2_USB_Init(void);

#ifdef __cplusplus
}
#endif
#endif /* __UART_H__ */
