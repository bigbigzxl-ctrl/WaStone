/* UART.c — minimal stub for USB CDC banner test (no real UART bridging) */
#include "UART.h"

UART_CTL Uart = {
    .Com_Cfg = { 0x00, 0xC2, 0x01, 0x00,   /* 115200 LE */
                 0x00,                       /* 1 stop bit */
                 0x00,                       /* no parity */
                 0x08,                       /* 8 data bits */
                 0x00 }
};

uint8_t UART2_Tx_Buf[DEF_UARTx_TX_BUF_LEN];
uint8_t UART2_Rx_Buf[DEF_UARTx_RX_BUF_LEN];

void UART2_USB_Init(void) { /* no-op: we don't bridge to a physical UART */ }
