/*
 * stm32h563rgt6_cec_mailbox — HDMI-CEC peripheral self-test
 *
 * CEC (Consumer Electronics Control) is the single-wire protocol on HDMI
 * pin 13 that allows devices to send commands to each other (e.g. TV remote
 * controlling a Blu-ray player). The STM32H563 includes a full CEC controller.
 *
 * This test verifies register accessibility without driving the CEC line:
 *   1. Enable CEC APB1L clock (RCC_APB1LENR bit28 = CECEN)
 *   2. Verify CFGR accessible
 *   3. Write CFGR with OAR (own address) = 0x1 and LSTN=1 (listen mode)
 *   4. Read back CFGR to verify
 *   5. Read ISR (interrupt/status)
 *
 * CEC_BASE = APB1PERIPH_BASE + 0x7000 = 0x40007000
 * RCC_APB1LENR = RCC_BASE + 0x09C, bit28 = CECEN
 *
 * CEC registers:
 *   CR   +0x00: bit0=CECEN (enable), bit1=TXSOM (start of message), bit2=TXEOM
 *   CFGR +0x04: bits[14:0]=SFT/RXTOL/BRESTP/BREGEN/LBPEGEN/BRDNOGEN/SFTOP,
 *               bits[30:16]=OAR (own address, 15-bit), bit31=LSTN (listen)
 *   TXDR +0x08: transmit data byte
 *   RXDR +0x0C: receive data byte (read-only)
 *   ISR  +0x10: status flags
 *   IER  +0x14: interrupt enable
 *
 * Note: CECEN (bit0 of CR) must NOT be set before configuring CFGR.
 *       CFGR is only writable when CECEN=0.
 *
 * FAIL codes:
 *   0xE001 — CEC CFGR inaccessible (reads 0xFFFFFFFF)
 *   0xE002 — CFGR write not retained
 *
 * detail0: [31:16]=ISR, [15:0]=CFGR readback
 */

#include <stdint.h>
#include "../ael_mailbox.h"

#define RCC_BASE        0x44020C00u
#define RCC_APB1LENR    (*(volatile uint32_t *)(RCC_BASE + 0x09Cu))

#define CEC_BASE        0x40007000u
#define CEC_CR          (*(volatile uint32_t *)(CEC_BASE + 0x00u))
#define CEC_CFGR        (*(volatile uint32_t *)(CEC_BASE + 0x04u))
#define CEC_ISR         (*(volatile uint32_t *)(CEC_BASE + 0x10u))

/* CFGR: OAR=1 (own address 0x01) at bits[30:16], LSTN=0 (no bus listening) */
#define CEC_CFGR_VAL  (1u << 16)  /* OAR[0]=bit16; LSTN=0 to avoid bus activity */

int main(void)
{
    ael_mailbox_init();

    /* 1. Enable CEC clock (APB1LENR bit28) */
    RCC_APB1LENR |= (1u << 28);
    (void)RCC_APB1LENR;

    /* 2. Verify accessibility (CECEN=0, so CFGR is writable) */
    uint32_t cfgr = CEC_CFGR;
    if (cfgr == 0xFFFFFFFFu) {
        ael_mailbox_fail(0xE001u, cfgr);
        while (1) {}
    }

    /* 3. Clear any pending ISR flags (W1C) to ensure clean state before config */
    CEC_ISR = 0xFFFFFFFFu;
    /* Write CFGR (only valid when CECEN=0) */
    CEC_CFGR = CEC_CFGR_VAL;
    (void)CEC_CFGR;

    /* 4. Read back and verify OAR bit (bit16) */
    cfgr = CEC_CFGR;
    if ((cfgr & 0x00010000u) != (CEC_CFGR_VAL & 0x00010000u)) {
        ael_mailbox_fail(0xE002u, cfgr);
        while (1) {}
    }

    /* 5. Read ISR */
    uint32_t isr = CEC_ISR;

    /* detail0: [31:16]=ISR[15:0], [15:0]=CFGR[31:16] (should have OAR[0]=bit0) */
    AEL_MAILBOX->detail0 = ((isr & 0xFFFFu) << 16) | ((cfgr >> 16) & 0xFFFFu);
    ael_mailbox_pass();
    while (1) {}
    return 0;
}
