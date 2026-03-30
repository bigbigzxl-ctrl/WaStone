/*
 * stm32g431_fdcan_loopback — FDCAN1 internal loopback self-test
 *
 * Uses FDCAN TEST.LBCK mode: Tx is internally looped back to Rx,
 * FDCAN1_TX pin held recessive, FDCAN1_RX pin disconnected.
 * No external CAN bus or transceiver required.
 *
 * What is verified:
 *   1. Clock + FDCAN1 initialisation correct
 *   2. Message RAM (STM32G4 fixed layout) accessible
 *   3. Standard ID filter passes the test frame into FIFO0
 *   4. Tx: frame queued and sent (TXBAR → TXBTO)
 *   5. Rx: frame received in FIFO0 with correct ID, DLC, data
 *
 * Message RAM layout (fixed for G4, per RM0440 §44.3.3 and libopencm3):
 *   FDCAN1_RAM = 0x4000A400
 *   +0x0000  Standard ID filter list  (28 × 4 B = 112 B)
 *   +0x0070  Extended ID filter list  ( 8 × 8 B =  64 B)
 *   +0x00B0  Rx FIFO0                 ( 3 × 72 B = 216 B)
 *   +0x0188  Rx FIFO1                 ( 3 × 72 B = 216 B)
 *   +0x0260  Tx Event FIFO            ( 3 × 8 B  =  24 B)
 *   +0x0278  Tx Buffer                ( 3 × 72 B = 216 B)
 *   Total per instance: 0x0350 B
 *
 * Each Rx / Tx element: 2 × uint32_t header + 16 × uint32_t data = 72 B.
 * (Sized for FDCAN 64-byte max payload; classic 8-byte uses first 2 data words.)
 *
 * Bit timing: PCLK1 = 16 MHz HSI, 500 kbps, 16 TQ per bit.
 *   NBRP=1(reg), NTSEG1=11(reg)→12 TQ, NTSEG2=2(reg)→3 TQ, NSJW=0(reg)→1 TQ
 *   NBTP = 0x00010B02
 */

#include <stdint.h>
#include "../ael_mailbox.h"

/* ── RCC ──────────────────────────────────────────────────────────── */
#define RCC_BASE        0x40021000u
#define RCC_APB1ENR1    (*(volatile uint32_t *)(RCC_BASE + 0x58u))
#define RCC_CCIPR       (*(volatile uint32_t *)(RCC_BASE + 0x88u))
/* RCC_APB1ENR1 bit 25 = FDCANEN */
/* RCC_CCIPR bits [25:24]: FDCANSEL — 10b = PCLK1 */

/* ── FDCAN1 registers (APB1, 0x40006400) ─────────────────────────── */
#define FDCAN1_BASE     0x40006400u
#define FDCAN1_TEST     (*(volatile uint32_t *)(FDCAN1_BASE + 0x0010u))
#define FDCAN1_CCCR     (*(volatile uint32_t *)(FDCAN1_BASE + 0x0018u))
#define FDCAN1_NBTP     (*(volatile uint32_t *)(FDCAN1_BASE + 0x001Cu))
#define FDCAN1_IR       (*(volatile uint32_t *)(FDCAN1_BASE + 0x0050u))
#define FDCAN1_RXGFC    (*(volatile uint32_t *)(FDCAN1_BASE + 0x0080u))
/* Rx FIFO0 status (fill level, get index) and acknowledge */
#define FDCAN1_RXF0S    (*(volatile uint32_t *)(FDCAN1_BASE + 0x0090u))
#define FDCAN1_RXF0A    (*(volatile uint32_t *)(FDCAN1_BASE + 0x0094u))
#define FDCAN1_TXBC     (*(volatile uint32_t *)(FDCAN1_BASE + 0x00C0u))
#define FDCAN1_TXFQS    (*(volatile uint32_t *)(FDCAN1_BASE + 0x00C4u))
#define FDCAN1_TXBRP    (*(volatile uint32_t *)(FDCAN1_BASE + 0x00C8u))
#define FDCAN1_TXBAR    (*(volatile uint32_t *)(FDCAN1_BASE + 0x00CCu))
#define FDCAN1_TXBTO    (*(volatile uint32_t *)(FDCAN1_BASE + 0x00D4u))

/* CCCR bits */
#define CCCR_INIT   (1u << 0)
#define CCCR_CCE    (1u << 1)
#define CCCR_TEST   (1u << 7)
/* TEST bits */
#define TEST_LBCK   (1u << 4)
/* NBTP: 500 kbps @ 16 MHz PCLK1
 *   NSJW[31:25]=0, NBRP[24:16]=1, NTSEG1[15:8]=11, NTSEG2[6:0]=2 */
#define NBTP_500KBPS  0x00010B02u
/* RXGFC: LSS=1 (1 std filter), ANFS=01 (non-matching std → FIFO0) */
#define RXGFC_SETUP   ((1u << 16) | (0x1u << 4))

/* ── FDCAN1 Message RAM (fixed layout, per RM0440) ──────────────── */
#define FDCAN1_RAM_BASE  0x4000A400u
/* Standard ID filter list at +0x0000 (28 × 4 B) */
#define FDCAN1_STDFILTER ((volatile uint32_t *)(FDCAN1_RAM_BASE + 0x0000u))
/* Rx FIFO0 at +0x00B0 (3 × 72 B, each element: 2 header + 16 data words) */
#define FDCAN1_RXF0_BASE (FDCAN1_RAM_BASE + 0x00B0u)
/* Tx Buffer at +0x0278 (3 × 72 B) */
#define FDCAN1_TXBUF_BASE (FDCAN1_RAM_BASE + 0x0278u)

/* Element size in words (2 header + 16 data = 18 uint32) — ALWAYS 72 B on G4 */
#define FDCAN_ELEMENT_WORDS  18u
#define FDCAN_ELEMENT_BYTES  (FDCAN_ELEMENT_WORDS * 4u)  /* 72 */

/* Standard filter element word (mask filter, accept all, → FIFO0):
 *   bits[31:30] = SFT   = 10b (classic mask filter)
 *   bits[29:27] = SFEC  = 001b (store in FIFO0)
 *   bits[26:16] = SFID1 = 0   (filter pattern, irrelevant with mask=0)
 *   bits[10:0]  = SFID2 = 0   (mask = 0 → accept all IDs) */
#define STDFILTER_PASS_ALL  ((0x2u << 30) | (0x1u << 27))

/* Test frame parameters */
#define TEST_FRAME_ID   0x123u  /* standard 11-bit CAN ID */
#define TEST_DLC        8u      /* 8 data bytes */

/* Tx element word 0 (identifier_flags): standard frame, no RTR/XTD
 *   Standard ID at bits[28:18] (FDCAN_FIFO_SID_SHIFT=18) */
#define TX_W0  ((TEST_FRAME_ID & 0x7FFu) << 18)
/* Tx element word 1 (evt_fmt_dlc_res): DLC at bits[19:16] */
#define TX_W1  ((TEST_DLC & 0xFu) << 16)

/* SysTick (for settling delay) */
#define SYST_CSR   (*(volatile uint32_t *)0xE000E010u)
#define SYST_RVR   (*(volatile uint32_t *)0xE000E014u)
#define SYST_CVR   (*(volatile uint32_t *)0xE000E018u)

/* ── GPIOA (for PA2 signal output) ──────────────────────────────── */
#define RCC_AHB2ENR (*(volatile uint32_t *)(RCC_BASE + 0x4Cu))
#define GPIOA_BASE  0x48000000u
#define GPIOA_MODER (*(volatile uint32_t *)(GPIOA_BASE + 0x00u))
#define GPIOA_OTYPER (*(volatile uint32_t *)(GPIOA_BASE + 0x04u))
#define GPIOA_OSPEEDR (*(volatile uint32_t *)(GPIOA_BASE + 0x08u))
#define GPIOA_ODR   (*(volatile uint32_t *)(GPIOA_BASE + 0x14u))

/* Error codes (stored in mailbox detail0) */
#define ERR_INIT_TIMEOUT   0xE001u  /* FDCAN INIT bit stuck */
#define ERR_RX_TIMEOUT     0xE002u  /* no frame in FIFO0 after send */
#define ERR_ID_MISMATCH    0xE003u  /* received ID ≠ TEST_FRAME_ID */
#define ERR_DLC_MISMATCH   0xE004u  /* received DLC ≠ TEST_DLC */
#define ERR_DATA_W0        0xE005u  /* data bytes 0-3 mismatch */
#define ERR_DATA_W1        0xE006u  /* data bytes 4-7 mismatch */

/* Sent data (little-endian words: DB0..DB3, DB4..DB7) */
#define TX_DATA_W0  0x04030201u
#define TX_DATA_W1  0x08070605u

static void delay_loop(uint32_t n)
{
    volatile uint32_t i = n;
    while (i-- > 0u) { (void)i; }
}

int main(void)
{
    uint32_t timeout;

    /* ── 1. Clocks ─────────────────────────────────────────────── */
    /* GPIOA for PA2 signal */
    RCC_AHB2ENR |= (1u << 0);
    /* FDCAN1 on APB1ENR1 bit 25 */
    RCC_APB1ENR1 |= (1u << 25);
    /* FDCANSEL = 10b (PCLK1 = 16 MHz HSI) in RCC_CCIPR bits[25:24] */
    RCC_CCIPR = (RCC_CCIPR & ~(0x3u << 24)) | (0x2u << 24);
    (void)RCC_CCIPR;   /* pipeline flush */

    /* ── 2. PA2 as signal output ───────────────────────────────── */
    GPIOA_MODER  &= ~(0x3u << 4);
    GPIOA_MODER  |=  (0x1u << 4);
    GPIOA_OTYPER &= ~(1u << 2);
    GPIOA_OSPEEDR|=  (0x3u << 4);
    GPIOA_ODR    &= ~(1u << 2);

    ael_mailbox_init();

    /* ── 3. Enter FDCAN1 initialisation + CCE mode ─────────────── */
    /* Set INIT; FDCAN may already be in INIT after reset but be safe */
    FDCAN1_CCCR |= CCCR_INIT;
    timeout = 0x0000FFFFu;
    while (((FDCAN1_CCCR & CCCR_INIT) == 0u) && (timeout-- > 0u)) {}
    if ((FDCAN1_CCCR & CCCR_INIT) == 0u) {
        ael_mailbox_fail(ERR_INIT_TIMEOUT, 0u);
        while (1) {}
    }
    FDCAN1_CCCR |= CCCR_CCE;   /* allow config register writes */

    /* ── 4. Configure nominal bit timing (500 kbps @ PCLK1=16 MHz) */
    FDCAN1_NBTP = NBTP_500KBPS;

    /* ── 5. Enable internal loopback test mode ─────────────────── */
    FDCAN1_CCCR |= CCCR_TEST;
    FDCAN1_TEST |= TEST_LBCK;

    /* ── 6. Configure Message RAM ──────────────────────────────── */
    /* 6a. 1 standard ID filter, ANFS=01 (non-matching → FIFO0) */
    FDCAN1_RXGFC = RXGFC_SETUP;

    /* 6b. Standard filter 0: classic mask, accept ALL IDs → FIFO0 */
    FDCAN1_STDFILTER[0] = STDFILTER_PASS_ALL;

    /* 6c. Zero-init the Tx buffer 0 header (optional safety clear) */
    volatile uint32_t *txbuf = (volatile uint32_t *)FDCAN1_TXBUF_BASE;
    txbuf[0] = TX_W0;       /* identifier_flags: standard ID, no XTD/RTR */
    txbuf[1] = TX_W1;       /* evt_fmt_dlc_res:  DLC=8 */
    txbuf[2] = TX_DATA_W0;  /* data bytes 0-3 */
    txbuf[3] = TX_DATA_W1;  /* data bytes 4-7 */
    /* bytes 8-63 not used; leave as-is */

    /* ── 7. Start FDCAN1 (leave INIT mode) ────────────────────── */
    FDCAN1_CCCR &= ~CCCR_INIT;
    timeout = 0x0000FFFFu;
    while (((FDCAN1_CCCR & CCCR_INIT) != 0u) && (timeout-- > 0u)) {}
    if ((FDCAN1_CCCR & CCCR_INIT) != 0u) {
        ael_mailbox_fail(ERR_INIT_TIMEOUT, 1u);
        while (1) {}
    }

    /* Brief settle (FDCAN synchronises to CAN bus / loopback) */
    delay_loop(1000u);

    /* ── 8. Transmit buffer 0 ──────────────────────────────────── */
    FDCAN1_TXBAR = (1u << 0);   /* add request for Tx buffer 0 */

    /* ── 9. Wait for Rx FIFO0 to have ≥ 1 frame ───────────────── */
    timeout = 2000000u;
    /* F0FL (fill level) in RXF0S bits [3:0] */
    while (((FDCAN1_RXF0S & 0xFu) == 0u) && (timeout-- > 0u)) {}

    if ((FDCAN1_RXF0S & 0xFu) == 0u) {
        ael_mailbox_fail(ERR_RX_TIMEOUT, (uint32_t)FDCAN1_IR);
        while (1) {}
    }

    /* ── 10. Read and validate received frame ──────────────────── */
    /* Get index of oldest received element */
    uint32_t get_idx = (FDCAN1_RXF0S >> 8) & 0x3u;  /* F0GI at bits[9:8] */

    /* Rx FIFO0 element address (each element = FDCAN_ELEMENT_BYTES = 72 B) */
    const volatile uint32_t *rxelem =
        (const volatile uint32_t *)(FDCAN1_RXF0_BASE +
                                    (get_idx * FDCAN_ELEMENT_BYTES));

    uint32_t rx_w0 = rxelem[0];  /* identifier_flags */
    uint32_t rx_w1 = rxelem[1];  /* filt_fmt_dlc_ts */
    uint32_t rx_d0 = rxelem[2];  /* data bytes 0-3 */
    uint32_t rx_d1 = rxelem[3];  /* data bytes 4-7 */

    /* Acknowledge (release FIFO slot) */
    FDCAN1_RXF0A = get_idx;

    /* Check: standard ID (no XTD flag) at bits[28:18] */
    if ((rx_w0 & (1u << 30)) != 0u) {
        /* XTD set — unexpected extended frame */
        ael_mailbox_fail(ERR_ID_MISMATCH, rx_w0);
        while (1) {}
    }
    uint32_t rx_id = (rx_w0 >> 18) & 0x7FFu;
    if (rx_id != TEST_FRAME_ID) {
        ael_mailbox_fail(ERR_ID_MISMATCH, rx_id);
        while (1) {}
    }

    /* Check: DLC at bits[19:16] of word 1 */
    uint32_t rx_dlc = (rx_w1 >> 16) & 0xFu;
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

    /* ── 11. PASS ───────────────────────────────────────────────── */
    ael_mailbox_pass();
    GPIOA_ODR |= (1u << 2);  /* PA2 HIGH */

    while (1) {}
}
