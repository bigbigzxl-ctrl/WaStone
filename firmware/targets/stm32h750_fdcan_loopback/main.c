/*
 * stm32h750_fdcan_loopback — FDCAN1 internal loopback Layer-1 test
 *
 * Verifies: FDCAN1 clock, CCCR init/CCE, NBTP bit timing, Message RAM
 * configuration, standard ID filter, Tx buffer transmission, Rx FIFO0
 * receive with ID/DLC/data integrity check.
 *
 * Internal loopback mode: CCCR.TEST=1, CCCR.MON=1, TEST.LBCK=1.
 * No external CAN transceiver or bus required.
 *
 * Key H750 differences vs STM32G431:
 *   FDCAN1_BASE = 0x4000A000 (G431: 0x40006400)
 *   SRAMCAN (shared Message RAM) = 0x4000AC00 (G431: fixed per-instance)
 *   H750 Message RAM is CONFIGURABLE via SIDFC/RXF0C/TXBC registers.
 *   GFC (not RXGFC) at +0x80 for non-matching frame routing.
 *   SIDFC at +0x084: std filter list start address + count.
 *   RXF0C at +0x0A0 (G431: +0x090).
 *   RXF0S at +0x0A4 (G431: +0x090).
 *   RXF0A at +0x0A8 (G431: +0x094).
 *   TXBC  at +0x0C0 (same).
 *   TXBAR at +0x0D0 (G431: +0x0CC).
 *   RXESC/TXESC at +0xBC/+0xC8: element data size (0=8-byte classic CAN).
 *   Each element: 2 header + 2 data words = 4 words = 16 bytes (for 8-byte CAN).
 *
 * FDCAN clock source: HSE (default FDCANSEL=00b in RCC_D2CCIP1R).
 *   YD-STM32H750VBT6 has 25 MHz HSE crystal. Must start HSE before using FDCAN.
 * NBTP for 500 kbps @ 25 MHz HSE, 10 TQ:
 *   BRP=5 (reg=4), NTSEG1=6 TQ (reg=5), NTSEG2=3 TQ (reg=2), NSJW=1 (reg=0)
 *   NBTP = 0x00040502
 *
 * Message RAM layout in SRAMCAN (starting at word 0):
 *   Word  0: Std filter[0]        (1 word × 1 filter)
 *   Words 1-4: RxFIFO0[0]        (4 words × 1 element = 16 bytes)
 *   Words 5-8: TxBuffer[0]       (4 words × 1 element = 16 bytes)
 *   Total: 9 words = 36 bytes
 *
 * Error codes:
 *   0xE001 = FDCAN INIT stuck (timeout entering/leaving init mode)
 *   0xE002 = Rx FIFO0 empty after Tx request (frame not received)
 *   0xE003 = ID mismatch (XTD set, or ID ≠ 0x123)
 *   0xE004 = DLC mismatch
 *   0xE005 = data bytes 0-3 mismatch
 *   0xE006 = data bytes 4-7 mismatch
 *
 * All register addresses from RM0433 + stm32h750xx.h (cmsis_device_h7).
 */

#include <stdint.h>
#define AEL_MAILBOX_ADDR  0x2000FF00u
#include "../ael_mailbox.h"

/* ── RCC ───────────────────────────────────────────────────────── */
#define RCC_BASE           0x58024400u
#define RCC_CR             (*(volatile uint32_t *)(RCC_BASE + 0x000u))
#define RCC_APB1HENR       (*(volatile uint32_t *)(RCC_BASE + 0x0ECu))
#define RCC_APB1HENR_FDCANEN (1u << 8)
/* HSE oscillator control in RCC_CR */
#define RCC_CR_HSEON   (1u << 16)
#define RCC_CR_HSERDY  (1u << 17)

/* ── FDCAN1 (APB1, D2 domain, base 0x4000A000) ──────────────────── */
#define FDCAN1_BASE   0x4000A000u
#define FDCAN1_CCCR   (*(volatile uint32_t *)(FDCAN1_BASE + 0x018u))
#define FDCAN1_TEST   (*(volatile uint32_t *)(FDCAN1_BASE + 0x010u))
#define FDCAN1_NBTP   (*(volatile uint32_t *)(FDCAN1_BASE + 0x01Cu))
#define FDCAN1_IR     (*(volatile uint32_t *)(FDCAN1_BASE + 0x050u))
/* Message RAM configuration registers (configurable, unlike G431 fixed layout) */
#define FDCAN1_GFC    (*(volatile uint32_t *)(FDCAN1_BASE + 0x080u))  /* global filter */
#define FDCAN1_SIDFC  (*(volatile uint32_t *)(FDCAN1_BASE + 0x084u))  /* std ID filter config */
#define FDCAN1_XIDFC  (*(volatile uint32_t *)(FDCAN1_BASE + 0x088u))  /* ext ID filter config */
#define FDCAN1_RXF0C  (*(volatile uint32_t *)(FDCAN1_BASE + 0x0A0u))  /* Rx FIFO0 config */
#define FDCAN1_RXF0S  (*(volatile uint32_t *)(FDCAN1_BASE + 0x0A4u))  /* Rx FIFO0 status */
#define FDCAN1_RXF0A  (*(volatile uint32_t *)(FDCAN1_BASE + 0x0A8u))  /* Rx FIFO0 ack */
#define FDCAN1_RXBC   (*(volatile uint32_t *)(FDCAN1_BASE + 0x0ACu))  /* Rx buffer config */
#define FDCAN1_RXESC  (*(volatile uint32_t *)(FDCAN1_BASE + 0x0BCu))  /* Rx element size */
#define FDCAN1_TXBC   (*(volatile uint32_t *)(FDCAN1_BASE + 0x0C0u))  /* Tx buffer config */
#define FDCAN1_TXESC  (*(volatile uint32_t *)(FDCAN1_BASE + 0x0C8u))  /* Tx element size */
#define FDCAN1_TXBAR  (*(volatile uint32_t *)(FDCAN1_BASE + 0x0D0u))  /* Tx buffer add req */

/* CCCR bits */
#define CCCR_INIT  (1u << 0)
#define CCCR_CCE   (1u << 1)
#define CCCR_MON   (1u << 5)   /* Bus Monitoring Mode — used with internal loopback */
#define CCCR_TEST  (1u << 7)
/* TEST bits */
#define TEST_LBCK  (1u << 4)

/* NBTP: 500 kbps @ 25 MHz HSE, 10 TQ
 *   BRP=5 (reg=4), NTSEG1=6 (reg=5), NTSEG2=3 (reg=2), NSJW=1 (reg=0)
 *   Bit time = 1 + 6 + 3 = 10 TQ; TQ = 1/(25MHz/5) = 200 ns → 500 kbps */
#define NBTP_500KBPS_25MHZ  0x00040502u

/* ── FDCAN1 Message RAM (SRAMCAN, shared, base 0x4000AC00) ─────── */
#define SRAMCAN_BASE  0x4000AC00u

/*
 * Layout (all addresses are word addresses relative to SRAMCAN_BASE,
 * multiplied by 4 for byte offsets):
 *
 *   Word 0 (byte  0): Std ID filter[0]         — 1 word  → 4 bytes
 *   Word 1 (byte  4): RxFIFO0[0] header word 0 — element start
 *   Word 2 (byte  8): RxFIFO0[0] header word 1
 *   Word 3 (byte 12): RxFIFO0[0] data word 0 (bytes 0-3)
 *   Word 4 (byte 16): RxFIFO0[0] data word 1 (bytes 4-7)
 *   Word 5 (byte 20): TxBuffer[0] header word 0 — element start
 *   Word 6 (byte 24): TxBuffer[0] header word 1
 *   Word 7 (byte 28): TxBuffer[0] data word 0 (bytes 0-3)
 *   Word 8 (byte 32): TxBuffer[0] data word 1 (bytes 4-7)
 *
 * SIDFC register: FLSSA[15:2] = byte_addr_of_start / 4 * 4 = 0, LSS[23:16] = 1.
 *   SIDFC = (0u) | (1u << 16) = 0x00010000
 * XIDFC: FLESA = byte offset 4 (word 1), LSE = 0 (no ext filters).
 *   XIDFC = (4u) | (0u << 16) = 0x00000004
 * RXF0C: F0SA = byte offset 4 (same as XIDFC start since LSE=0), F0S = 1.
 *   RXF0C = (4u) | (1u << 16) = 0x00010004
 * TXBC:  TBSA = byte offset 20, NDTB = 1 (1 dedicated Tx buffer).
 *   TXBC = (20u) | (1u << 16) = 0x00010014
 *
 * RXESC = 0 (F0DS=0 → 8-byte data field → element = 2+2 = 4 words)
 * TXESC = 0 (TBDS=0 → 8-byte data field)
 */
#define FDCAN1_STDFILTER  ((volatile uint32_t *)(SRAMCAN_BASE + 0x0000u))
#define FDCAN1_RXF0_BASE  (SRAMCAN_BASE + 0x0004u)   /* RxFIFO0 element 0 */
#define FDCAN1_TXBUF_BASE (SRAMCAN_BASE + 0x0014u)   /* TxBuffer element 0 */
#define FDCAN_ELEMENT_BYTES 16u  /* 4 words × 4 bytes = 16 bytes per element */

/* Standard filter element: classic mask, accept ALL IDs → FIFO0
 *   bits[31:30]=SFT=10b (mask), bits[29:27]=SFEC=001b (FIFO0), SFID1=0, SFID2=0 */
#define STDFILTER_PASS_ALL  ((0x2u << 30u) | (0x1u << 27u))

/* Test frame */
#define TEST_FRAME_ID  0x123u
#define TEST_DLC       8u
/* Tx element word 0: std ID at bits[28:18], XTD=0, RTR=0 */
#define TX_W0  ((TEST_FRAME_ID & 0x7FFu) << 18u)
/* Tx element word 1: DLC at bits[19:16] */
#define TX_W1  ((TEST_DLC & 0xFu) << 16u)
/* Data: 0x01,0x02,...,0x08 packed little-endian */
#define TX_DATA_W0  0x04030201u
#define TX_DATA_W1  0x08070605u

/* Error codes */
#define ERR_INIT_TIMEOUT  0xE001u
#define ERR_RX_TIMEOUT    0xE002u
#define ERR_ID_MISMATCH   0xE003u
#define ERR_DLC_MISMATCH  0xE004u
#define ERR_DATA_W0       0xE005u
#define ERR_DATA_W1       0xE006u

static void delay_loop(volatile uint32_t n)
{
    while (n-- > 0u) { (void)n; }
}

int main(void)
{
    uint32_t timeout;

    /* ── 1. Start HSE (25 MHz crystal on YD-H750 board) ──────────
     * FDCAN default clock source = HSE (RCC_D2CCIP1R FDCANSEL=00b).
     * HSE must be started explicitly; HSI is the power-on default. */
    RCC_CR |= RCC_CR_HSEON;
    timeout = 0x000FFFFFu;
    while (((RCC_CR & RCC_CR_HSERDY) == 0u) && (timeout-- > 0u)) {}
    if ((RCC_CR & RCC_CR_HSERDY) == 0u) {
        /* Fallback: if no HSE crystal, flag init timeout for diagnosis */
        ael_mailbox_init();
        ael_mailbox_fail(ERR_INIT_TIMEOUT, 0xDEAD0001u);
        while (1) {}
    }

    /* ── 2. Enable FDCAN1 APB1H clock ────────────────────────────── */
    RCC_APB1HENR |= RCC_APB1HENR_FDCANEN;
    (void)RCC_APB1HENR;

    ael_mailbox_init();

    /* ── 3. Enter FDCAN1 initialisation mode (INIT=1) ────────────── */
    FDCAN1_CCCR |= CCCR_INIT;
    timeout = 0x0000FFFFu;
    while (((FDCAN1_CCCR & CCCR_INIT) == 0u) && (timeout-- > 0u)) {}
    if ((FDCAN1_CCCR & CCCR_INIT) == 0u) {
        ael_mailbox_fail(ERR_INIT_TIMEOUT, 1u);
        while (1) {}
    }

    /* ── 4. Enable Configuration Change Enable (CCE=1) ───────────── */
    FDCAN1_CCCR |= CCCR_CCE;

    /* ── 5. Configure nominal bit timing: 500 kbps @ 25 MHz HSE ──── */
    FDCAN1_NBTP = NBTP_500KBPS_25MHZ;

    /* ── 6. Enable internal loopback (TEST + MON + TEST.LBCK) ────── */
    FDCAN1_CCCR |= CCCR_TEST | CCCR_MON;
    FDCAN1_TEST |= TEST_LBCK;

    /* ── 7. Configure Message RAM ─────────────────────────────────── */
    /* GFC: ANFS=00 (non-matching std → FIFO0) — leave at reset default (0) */
    FDCAN1_GFC = 0u;

    /* SIDFC: std filter list at byte 0 (word 0), 1 filter */
    FDCAN1_SIDFC = (0u) | (1u << 16u);   /* FLSSA=0, LSS=1 */

    /* XIDFC: ext filter list starts at byte 4 (word 1), 0 ext filters */
    FDCAN1_XIDFC = (4u) | (0u << 16u);   /* FLESA=4, LSE=0 */

    /* RXF0C: FIFO0 starts at byte 4 (word 1), 1 element */
    FDCAN1_RXF0C = (4u) | (1u << 16u);   /* F0SA=4, F0S=1 */

    /* RXBC: no dedicated Rx buffers */
    FDCAN1_RXBC = 0u;

    /* RXESC: F0DS=0 → 8-byte data (4 words per element) */
    FDCAN1_RXESC = 0u;

    /* TXBC: Tx buffer starts at byte 20 (word 5), 1 dedicated buffer */
    FDCAN1_TXBC = (20u) | (1u << 16u);   /* TBSA=20, NDTB=1 */

    /* TXESC: TBDS=0 → 8-byte data */
    FDCAN1_TXESC = 0u;

    /* ── 8. Write std filter[0]: classic mask, accept all → FIFO0 ── */
    FDCAN1_STDFILTER[0] = STDFILTER_PASS_ALL;

    /* ── 9. Write Tx buffer[0] element ────────────────────────────── */
    volatile uint32_t *txbuf = (volatile uint32_t *)FDCAN1_TXBUF_BASE;
    txbuf[0] = TX_W0;       /* identifier_flags: std ID 0x123, no XTD/RTR */
    txbuf[1] = TX_W1;       /* evt_fmt_dlc_res: DLC=8, classic CAN */
    txbuf[2] = TX_DATA_W0;  /* data bytes 0-3 */
    txbuf[3] = TX_DATA_W1;  /* data bytes 4-7 */

    /* ── 10. Leave INIT mode (start FDCAN) ────────────────────────── */
    FDCAN1_CCCR &= ~CCCR_INIT;
    timeout = 0x0000FFFFu;
    while (((FDCAN1_CCCR & CCCR_INIT) != 0u) && (timeout-- > 0u)) {}
    if ((FDCAN1_CCCR & CCCR_INIT) != 0u) {
        ael_mailbox_fail(ERR_INIT_TIMEOUT, 2u);
        while (1) {}
    }

    /* Brief settle for FDCAN to synchronise to internal loopback */
    delay_loop(1000u);

    /* ── 11. Transmit Tx buffer 0 ─────────────────────────────────── */
    FDCAN1_TXBAR = (1u << 0u);   /* add request for buffer 0 */

    /* ── 12. Wait for frame in Rx FIFO0 ──────────────────────────── */
    /* F0FL (fill level) in RXF0S bits [6:0] */
    timeout = 2000000u;
    while (((FDCAN1_RXF0S & 0x7Fu) == 0u) && (timeout-- > 0u)) {}

    if ((FDCAN1_RXF0S & 0x7Fu) == 0u) {
        ael_mailbox_fail(ERR_RX_TIMEOUT, (uint32_t)FDCAN1_IR);
        while (1) {}
    }

    /* ── 13. Read and validate received element ───────────────────── */
    /* F0GI (get index) at bits[13:8] */
    uint32_t get_idx = (FDCAN1_RXF0S >> 8u) & 0x3Fu;
    const volatile uint32_t *rxelem =
        (const volatile uint32_t *)(FDCAN1_RXF0_BASE +
                                    (get_idx * FDCAN_ELEMENT_BYTES));

    uint32_t rx_w0 = rxelem[0];
    uint32_t rx_w1 = rxelem[1];
    uint32_t rx_d0 = rxelem[2];
    uint32_t rx_d1 = rxelem[3];

    /* Acknowledge (release FIFO slot) */
    FDCAN1_RXF0A = get_idx;

    /* Check standard frame (XTD bit 30 must be 0) */
    if ((rx_w0 & (1u << 30u)) != 0u) {
        ael_mailbox_fail(ERR_ID_MISMATCH, rx_w0);
        while (1) {}
    }
    uint32_t rx_id = (rx_w0 >> 18u) & 0x7FFu;
    if (rx_id != TEST_FRAME_ID) {
        ael_mailbox_fail(ERR_ID_MISMATCH, rx_id);
        while (1) {}
    }

    /* Check DLC at bits[19:16] of rx_w1 */
    uint32_t rx_dlc = (rx_w1 >> 16u) & 0xFu;
    if (rx_dlc != TEST_DLC) {
        ael_mailbox_fail(ERR_DLC_MISMATCH, rx_dlc);
        while (1) {}
    }

    /* Check data */
    if (rx_d0 != TX_DATA_W0) {
        ael_mailbox_fail(ERR_DATA_W0, rx_d0);
        while (1) {}
    }
    if (rx_d1 != TX_DATA_W1) {
        ael_mailbox_fail(ERR_DATA_W1, rx_d1);
        while (1) {}
    }

    /* ── 14. PASS ─────────────────────────────────────────────────── */
    ael_mailbox_pass();
    while (1) {}
}
