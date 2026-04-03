/*
 * stm32h563rgt6_fdcan_mailbox — FDCAN1 internal loopback self-test
 *
 * Uses FDCAN1 hardware loopback mode (CCCR.TEST=1, TEST.LBCK=1).
 * No external CAN bus wiring required.
 *
 * Sends one classic CAN frame (11-bit ID=0x123, DLC=4, data={0x11,0x22,0x33,0x44})
 * and verifies the received frame from SRAM message RAM (Rx FIFO0).
 *
 * FDCAN1_BASE    = APB1 + 0xA400 = 0x4000A400
 * SRAMCAN_BASE   = APB1 + 0xAC00 = 0x4000AC00
 * RCC_APB1HENR   = RCC_BASE + 0x0A0, bit9 = FDCANEN
 *
 * H563 FDCAN message RAM layout (FIXED, not configurable):
 *   Standard filter list:  28 × 4 B = 112 B  → offset 0x000
 *   Extended filter list:   8 × 8 B =  64 B  → offset 0x070
 *   Rx FIFO0:               3 × 72 B = 216 B  → offset 0x0B0
 *   Rx FIFO1:               3 × 72 B = 216 B  → offset 0x188
 *   Tx Event FIFO:          3 × 8 B  =  24 B  → offset 0x260
 *   Tx FIFO/Queue:          3 × 72 B = 216 B  → offset 0x278
 *
 * Each Rx/Tx element is 18 words (72 bytes):
 *   word[0] = T0/R0: header word 0 — for std ID: ID<<18
 *   word[1] = T1/R1: header word 1 — DLC<<16 (0=0B,4=4B,8=8B)
 *   word[2..17]: data bytes (LE packing, 4 bytes per word)
 *
 * RXGFC register:
 *   bits[3:2]  = ANFE (accept non-matching extended): 2 → Rx FIFO0
 *   bits[5:4]  = ANFS (accept non-matching standard): 2 → Rx FIFO0
 *   bits[20:16]= LSS (standard filter count): 0 (no filters)
 *   bits[27:24]= LSE (extended filter count): 0 (no filters)
 *
 * TXBC register (H563): only bit24 = TFQM (0=dedicated, 1=queue)
 *   No TBSA or NDTB fields — Tx buffer location is FIXED at 0x278.
 *
 * IR register bit positions:
 *   bit0 = RF0N (Rx FIFO0 new message)
 *   bit7 = TC   (transmission complete)
 *
 * Nominal bit timing for 250 kbit/s @ 64 MHz input clock:
 *   Prescaler=8, TSEG1=15, TSEG2=4, SJW=4
 *   NBTP = (SJW-1)<<25 | (TSEG2-1)<<20 | (TSEG1-1)<<8 | (prescaler-1)
 *        = 3<<25 | 3<<20 | 14<<8 | 7 = 0x06030E07
 *
 * FAIL codes:
 *   0xE001 — FDCAN stuck in INIT (CCE not granted)
 *   0xE002 — Tx timeout (TXBAR set but TC never fires)
 *   0xE003 — Rx FIFO0 empty after Tx (loopback frame not received)
 *   0xE004 — data mismatch, detail0 = received word2 (data bytes 0-3)
 */

#include <stdint.h>
#include "../ael_mailbox.h"

#define RCC_BASE        0x44020C00u
#define RCC_APB1HENR    (*(volatile uint32_t *)(RCC_BASE + 0x0A0u))

/* FDCAN1 */
#define FDCAN1_BASE     0x4000A400u
#define FDCAN_CCCR      (*(volatile uint32_t *)(FDCAN1_BASE + 0x018u))
#define FDCAN_TEST      (*(volatile uint32_t *)(FDCAN1_BASE + 0x010u))
#define FDCAN_NBTP      (*(volatile uint32_t *)(FDCAN1_BASE + 0x01Cu))
#define FDCAN_RXGFC     (*(volatile uint32_t *)(FDCAN1_BASE + 0x080u))
#define FDCAN_RXF0S     (*(volatile uint32_t *)(FDCAN1_BASE + 0x090u))
#define FDCAN_RXF0A     (*(volatile uint32_t *)(FDCAN1_BASE + 0x094u))
#define FDCAN_TXBC      (*(volatile uint32_t *)(FDCAN1_BASE + 0x0C0u))
#define FDCAN_TXFQS     (*(volatile uint32_t *)(FDCAN1_BASE + 0x0C4u))
#define FDCAN_TXBAR     (*(volatile uint32_t *)(FDCAN1_BASE + 0x0CCu))
#define FDCAN_IR        (*(volatile uint32_t *)(FDCAN1_BASE + 0x050u))

#define FDCAN_CCCR_INIT  (1u << 0)
#define FDCAN_CCCR_CCE   (1u << 1)
#define FDCAN_CCCR_TEST  (1u << 7)
#define FDCAN_TEST_LBCK  (1u << 4)
#define FDCAN_IR_RF0N    (1u << 0)   /* Rx FIFO0 new message */
#define FDCAN_IR_TC      (1u << 7)   /* Transmission Complete (bit7, NOT bit9) */

/* SRAMCAN message RAM base */
#define SRAMCAN_BASE    0x4000AC00u

/* Fixed RAM offsets (byte offsets from SRAMCAN_BASE):
 *   Std filter list: 0x000 (28 × 4 B)
 *   Ext filter list: 0x070 (8 × 8 B)
 *   Rx FIFO0:        0x0B0 (3 × 72 B)
 *   Rx FIFO1:        0x188 (3 × 72 B)
 *   Tx Event FIFO:   0x260 (3 × 8 B)
 *   Tx FIFO/Queue:   0x278 (3 × 72 B)
 */
#define SRAMCAN_RF0SA   0x0B0u   /* Rx FIFO0 start (byte offset) */
#define SRAMCAN_TFQSA   0x278u   /* Tx FIFO/Queue start (byte offset) */
#define SRAMCAN_EL_SIZE 72u      /* element size in bytes (18 words × 4) */

/* Nominal bit timing: 250 kbit/s @ 64 MHz, prescaler=8, tseg1=15, tseg2=4, sjw=4 */
#define FDCAN_NBTP_VAL  ((3u<<25) | (3u<<20) | (14u<<8) | 7u)

/* RXGFC: ANFE=2 (non-matching extended→FIFO0): bits[3:2]
 *         ANFS=2 (non-matching standard→FIFO0): bits[5:4] */
#define FDCAN_RXGFC_VAL ((2u<<2) | (2u<<4))

#define TIMEOUT  1000000u

int main(void)
{
    ael_mailbox_init();

    /* 1. Enable FDCAN clock (APB1HENR bit9) */
    RCC_APB1HENR |= (1u << 9);
    (void)RCC_APB1HENR;

    /* 2. Request initialisation mode */
    FDCAN_CCCR = FDCAN_CCCR_INIT;
    uint32_t t;
    for (t = 0u; t < TIMEOUT; t++) {
        if (FDCAN_CCCR & FDCAN_CCCR_INIT) break;
    }

    /* 3. Enable configuration change access */
    FDCAN_CCCR |= FDCAN_CCCR_CCE;
    for (t = 0u; t < TIMEOUT; t++) {
        if ((FDCAN_CCCR & (FDCAN_CCCR_INIT | FDCAN_CCCR_CCE)) ==
            (FDCAN_CCCR_INIT | FDCAN_CCCR_CCE)) break;
    }
    if (t == TIMEOUT) {
        ael_mailbox_fail(0xE001u, FDCAN_CCCR);
        while (1) {}
    }

    /* 4. Enable internal loopback (TEST=1 in CCCR unlocks TEST register) */
    FDCAN_CCCR |= FDCAN_CCCR_TEST;
    FDCAN_TEST  = FDCAN_TEST_LBCK;

    /* 5. Nominal bit timing: 250 kbit/s @ 64 MHz */
    FDCAN_NBTP = FDCAN_NBTP_VAL;

    /* 6. Global filter: accept all non-matching std and ext to Rx FIFO0 */
    FDCAN_RXGFC = FDCAN_RXGFC_VAL;

    /* 7. Tx buffer config: dedicated mode (TFQM=0), 3 dedicated buffers (fixed in H563) */
    FDCAN_TXBC = 0u;

    /* 8. Exit initialisation mode */
    FDCAN_CCCR &= ~FDCAN_CCCR_INIT;
    for (t = 0u; t < TIMEOUT; t++) {
        if (!(FDCAN_CCCR & FDCAN_CCCR_INIT)) break;
    }

    /* 9. Write Tx message to SRAM (Tx FIFO/Queue start = SRAMCAN_BASE + 0x278):
     *    11-bit standard ID = 0x123 → placed in T0 bits[28:18]
     *    DLC = 4 bytes → T1 = (4 << 16)
     *    data = {0x11, 0x22, 0x33, 0x44} = 0x44332211 (LE)
     */
    volatile uint32_t *tx = (volatile uint32_t *)(SRAMCAN_BASE + SRAMCAN_TFQSA);
    tx[0] = (0x123u << 18);     /* T0: std 11-bit ID */
    tx[1] = (4u << 16);         /* T1: DLC=4, classic CAN */
    tx[2] = 0x44332211u;        /* data bytes 0-3 */
    tx[3] = 0u;

    /* 10. Clear IR, then request transmission of Tx buffer 0 */
    FDCAN_IR  = 0xFFFFFFFFu;
    FDCAN_TXBAR = (1u << 0);

    /* 11. Wait for TC (bit7 in IR) */
    for (t = 0u; t < TIMEOUT; t++) {
        if (FDCAN_IR & FDCAN_IR_TC) break;
    }
    if (t == TIMEOUT) {
        ael_mailbox_fail(0xE002u, FDCAN_IR);
        while (1) {}
    }

    /* 12. Check Rx FIFO0 fill level (bits[3:0] of RXF0S) */
    uint32_t rxf0s = FDCAN_RXF0S;
    if ((rxf0s & 0xFu) == 0u) {
        ael_mailbox_fail(0xE003u, rxf0s);
        while (1) {}
    }

    /* 13. Read received element:
     *     get_idx = F0GI = bits[9:8] of RXF0S
     *     element base = SRAMCAN_BASE + 0x0B0 + get_idx * 72
     *     data = element[2] (word[2] = bytes 8..11 = data 0-3)
     */
    uint32_t get_idx = (rxf0s >> 8) & 0x3u;
    volatile uint32_t *rx = (volatile uint32_t *)
        (SRAMCAN_BASE + SRAMCAN_RF0SA + get_idx * SRAMCAN_EL_SIZE);
    uint32_t rx_data = rx[2];
    FDCAN_RXF0A = get_idx;

    if (rx_data != 0x44332211u) {
        ael_mailbox_fail(0xE004u, rx_data);
        while (1) {}
    }

    AEL_MAILBOX->detail0 = rx_data;
    ael_mailbox_pass();
    while (1) {}
    return 0;
}
