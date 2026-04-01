/*
 * STM32F407 — CAN1 Internal Loopback Test
 *
 * CAN1 in Loop Back Mode (BTR.LBKM=1): transmitted frame echoed to RX FIFO0
 * internally — no external transceiver or wiring required.
 *
 * Pins: PB8=CAN1_RX AF9, PB9=CAN1_TX AF9 (configured but unused in loopback)
 *
 * Bit rate: 500 kbps from 16 MHz APB1
 *   BRP=1 (tq=125 ns), TS1=10, TS2=3  → 1+11+4=16 TQ → 500 kbps
 *
 * Filter: bank 0, 32-bit mask mode, F0R1=0, F0R2=0 → accept all stdid
 *
 * Test: transmit 8-byte frame (stdid=0x123), receive from FIFO0, compare
 *
 * Mailbox: 0x2001FC00
 *   PASS: detail0 = 8 (bytes matched)
 *   FAIL: error_code=1 CAN init timeout (INAK not set)
 *         error_code=2 CAN normal mode timeout (INAK not cleared)
 *         error_code=3 TX timeout (TXOK0 not set in TSR)
 *         error_code=4 RX FIFO0 empty after TX (FMP0=0)
 *         error_code=5 data mismatch; detail0=byte index
 */

#include <stdint.h>

#define AEL_MAILBOX_ADDR 0x2001FC00u
#include "ael_mailbox.h"

void HardFault_Handler(void)
{
    *((volatile uint32_t *)0xE000ED0Cu) = 0x05FA0004u;
    while (1) {}
}

/* ---- RCC ---- */
#define RCC_BASE     0x40023800u
#define RCC_AHB1ENR  (*(volatile uint32_t *)(RCC_BASE + 0x30u))
#define RCC_APB1ENR  (*(volatile uint32_t *)(RCC_BASE + 0x40u))

/* ---- GPIOB ---- */
#define GPIOB_BASE   0x40020400u
#define GPIOB_MODER  (*(volatile uint32_t *)(GPIOB_BASE + 0x00u))
#define GPIOB_AFRH   (*(volatile uint32_t *)(GPIOB_BASE + 0x24u))

/* ---- CAN1 ---- */
#define CAN1_BASE    0x40006400u
#define CAN_MCR      (*(volatile uint32_t *)(CAN1_BASE + 0x000u))
#define CAN_MSR      (*(volatile uint32_t *)(CAN1_BASE + 0x004u))
#define CAN_TSR      (*(volatile uint32_t *)(CAN1_BASE + 0x008u))
#define CAN_RF0R     (*(volatile uint32_t *)(CAN1_BASE + 0x00Cu))
#define CAN_BTR      (*(volatile uint32_t *)(CAN1_BASE + 0x01Cu))

/* TX mailbox 0 */
#define CAN_TI0R     (*(volatile uint32_t *)(CAN1_BASE + 0x180u))
#define CAN_TDT0R    (*(volatile uint32_t *)(CAN1_BASE + 0x184u))
#define CAN_TDL0R    (*(volatile uint32_t *)(CAN1_BASE + 0x188u))
#define CAN_TDH0R    (*(volatile uint32_t *)(CAN1_BASE + 0x18Cu))

/* RX FIFO 0 */
#define CAN_RI0R     (*(volatile uint32_t *)(CAN1_BASE + 0x1B0u))
#define CAN_RDT0R    (*(volatile uint32_t *)(CAN1_BASE + 0x1B4u))
#define CAN_RDL0R    (*(volatile uint32_t *)(CAN1_BASE + 0x1B8u))
#define CAN_RDH0R    (*(volatile uint32_t *)(CAN1_BASE + 0x1BCu))

/* Filter banks */
#define CAN_FMR      (*(volatile uint32_t *)(CAN1_BASE + 0x200u))
#define CAN_FM1R     (*(volatile uint32_t *)(CAN1_BASE + 0x204u))
#define CAN_FS1R     (*(volatile uint32_t *)(CAN1_BASE + 0x20Cu))
#define CAN_FFA1R    (*(volatile uint32_t *)(CAN1_BASE + 0x214u))
#define CAN_FA1R     (*(volatile uint32_t *)(CAN1_BASE + 0x21Cu))
#define CAN_F0R1     (*(volatile uint32_t *)(CAN1_BASE + 0x240u))
#define CAN_F0R2     (*(volatile uint32_t *)(CAN1_BASE + 0x244u))

/* MCR bits */
#define CAN_MCR_INRQ   (1u << 0)
#define CAN_MCR_SLEEP  (1u << 1)
#define CAN_MCR_ABOM   (1u << 6)
/* MSR bits */
#define CAN_MSR_INAK   (1u << 0)
#define CAN_MSR_SLAK   (1u << 1)
/* TSR bits */
#define CAN_TSR_RQCP0  (1u << 0)
#define CAN_TSR_TXOK0  (1u << 1)
/* RF0R bits */
#define CAN_RF0R_FMP0_MASK  (3u << 0)
#define CAN_RF0R_RFOM0      (1u << 5)
/* BTR bits */
#define CAN_BTR_LBKM   (1u << 30)

static const uint8_t TX_DATA[8] = {0xDE, 0xAD, 0xBE, 0xEF,
                                    0xCA, 0xFE, 0xBA, 0xBE};

int main(void)
{
    ael_mailbox_init();

    /* ---- Enable clocks: GPIOB (AHB1 bit1), CAN1 (APB1 bit25) ---- */
    RCC_AHB1ENR |= (1u << 1);
    RCC_APB1ENR |= (1u << 25);
    (void)RCC_APB1ENR;

    /* ---- PB8=CAN1_RX AF9, PB9=CAN1_TX AF9 ---- */
    /* MODER: PB8[17:16]=10, PB9[19:18]=10 */
    GPIOB_MODER &= ~(0xFu << 16);
    GPIOB_MODER |=  (0xAu << 16);
    /* AFRH: PB8[3:0]=9, PB9[7:4]=9 */
    GPIOB_AFRH &= ~(0xFFu << 0);
    GPIOB_AFRH |=  (0x99u << 0);

    /* ---- Enter CAN init mode ---- */
    CAN_MCR &= ~CAN_MCR_SLEEP;   /* exit sleep */
    CAN_MCR |= CAN_MCR_INRQ;     /* request init */
    uint32_t timeout = 500000u;
    while (!(CAN_MSR & CAN_MSR_INAK)) {
        if (--timeout == 0u) { ael_mailbox_fail(1u, 0u); while (1) {} }
    }

    /* ---- Configure BTR: loopback, 500 kbps from 16 MHz APB1 ----
     * BRP=1: tq = (1+1)/16MHz = 125 ns
     * TS1=10: BS1 = 11 tq; TS2=3: BS2 = 4 tq
     * Total = 1+11+4 = 16 tq; rate = 1/(16*125ns) = 500 kbps
     */
    CAN_BTR = CAN_BTR_LBKM
            | (0u << 24)   /* SJW=0 (1 tq) */
            | (3u << 20)   /* TS2=3 */
            | (10u << 16)  /* TS1=10 */
            | (1u << 0);   /* BRP=1 */

    /* ---- Configure filter bank 0: 32-bit mask, accept all, FIFO0 ---- */
    CAN_FMR  |= (1u << 0);  /* FINIT: enter filter init */
    CAN_FS1R |= (1u << 0);  /* filter 0: 32-bit scale */
    CAN_FM1R &= ~(1u << 0); /* filter 0: mask mode */
    CAN_FFA1R &= ~(1u << 0);/* filter 0 → FIFO0 */
    CAN_F0R1  = 0u;          /* accept all IDs */
    CAN_F0R2  = 0u;
    CAN_FA1R |= (1u << 0);  /* activate filter 0 */
    CAN_FMR  &= ~(1u << 0); /* leave filter init */

    /* ---- Enter normal mode ---- */
    CAN_MCR &= ~CAN_MCR_INRQ;
    timeout = 500000u;
    while (CAN_MSR & CAN_MSR_INAK) {
        if (--timeout == 0u) { ael_mailbox_fail(2u, 0u); while (1) {} }
    }

    /* ---- Transmit frame: stdid=0x123, DLC=8 ---- */
    CAN_TDL0R = ((uint32_t)TX_DATA[0])
              | ((uint32_t)TX_DATA[1] << 8)
              | ((uint32_t)TX_DATA[2] << 16)
              | ((uint32_t)TX_DATA[3] << 24);
    CAN_TDH0R = ((uint32_t)TX_DATA[4])
              | ((uint32_t)TX_DATA[5] << 8)
              | ((uint32_t)TX_DATA[6] << 16)
              | ((uint32_t)TX_DATA[7] << 24);
    CAN_TDT0R = 8u;            /* DLC=8 */
    CAN_TI0R  = (0x123u << 21) /* STID */
              | (1u << 0);     /* TXRQ */

    /* ---- Wait TX complete ---- */
    timeout = 1000000u;
    while (!(CAN_TSR & CAN_TSR_RQCP0)) {
        if (--timeout == 0u) { ael_mailbox_fail(3u, CAN_TSR); while (1) {} }
    }
    if (!(CAN_TSR & CAN_TSR_TXOK0)) {
        ael_mailbox_fail(3u, CAN_TSR);
        while (1) {}
    }

    /* ---- Check RX FIFO0 has a message ---- */
    timeout = 500000u;
    while (!(CAN_RF0R & CAN_RF0R_FMP0_MASK)) {
        if (--timeout == 0u) { ael_mailbox_fail(4u, CAN_RF0R); while (1) {} }
    }

    /* ---- Read received data ---- */
    uint32_t rdl = CAN_RDL0R;
    uint32_t rdh = CAN_RDH0R;
    uint8_t rx[8];
    rx[0] = (uint8_t)(rdl >>  0);
    rx[1] = (uint8_t)(rdl >>  8);
    rx[2] = (uint8_t)(rdl >> 16);
    rx[3] = (uint8_t)(rdl >> 24);
    rx[4] = (uint8_t)(rdh >>  0);
    rx[5] = (uint8_t)(rdh >>  8);
    rx[6] = (uint8_t)(rdh >> 16);
    rx[7] = (uint8_t)(rdh >> 24);

    /* Release FIFO0 */
    CAN_RF0R |= CAN_RF0R_RFOM0;

    /* ---- Verify ---- */
    for (uint32_t i = 0u; i < 8u; i++) {
        if (rx[i] != TX_DATA[i]) {
            ael_mailbox_fail(5u, i);
            while (1) {}
        }
    }

    ael_mailbox_pass();
    AEL_MAILBOX->detail0 = 8u;
    while (1) {}
}
