/* ch32v305_usart_halfduplex — USART1 HDSEL + GPIO bit-bang echo
 * Board: CH32V305RBT6
 * Wiring: PA9↔PA10 jumper (Stage-2 UART jumper, already in place).
 *
 * CH32V305 USART HDSEL mode disables the receiver while transmitting,
 * so self-echo (same USART, same byte) is not possible without an
 * external echo source.
 *
 * Design:
 *   USART1 in HDSEL mode on PA9 (AF_OD): the half-duplex "node A".
 *   PA10: configured as GPIO IPU during TX (provides pull-up for the
 *         open-drain PA9 line), then switched to GPIO Out_PP to
 *         bit-bang the echo byte back via the PA9↔PA10 jumper.
 *   TIM2: used for accurate 4800-baud bit-period timing (20000 cycles
 *         per bit at 96 MHz, APB1 timer input = 2 × 48 MHz = 96 MHz).
 *
 * Test flow (per byte):
 *   1. PA10 = IPU  (weak pull-up for USART1 HDSEL TX)
 *   2. USART1 transmits byte on PA9; wait TC
 *   3. PA10 → GPIO Out_PP HIGH  (idle)
 *   4. Bit-bang same byte on PA10 (8N1, 4800 baud)
 *   5. USART1 (now in RX mode on PA9) receives echo; compare
 *
 * Fail codes written to mailbox:
 *   1 = data mismatch  (detail0 = byte index)
 *   3 = RX timeout     (detail0[31:16]=USART1_SR, detail0[15:0]=index)
 */
#define AEL_MAILBOX_ADDR 0x20000600u
#include "ael_mailbox.h"
#include "ch32v30x.h"

#define BUF_SIZE 4
static const uint8_t TxBuf[BUF_SIZE] = {0xA5, 0x5A, 0x12, 0x34};

/* --------------- TIM2 bit-timer helpers ------------------- */
/* APB1 timer input clock = 2 × PCLK1 = 2 × 48 MHz = 96 MHz  */
/* 4800 baud → bit period = 96 MHz / 4800 = 20 000 cycles     */

static void tim2_init(void)
{
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2, ENABLE);
    TIM2->PSC    = 0;
    TIM2->ATRLR  = 20000 - 1;   /* ARR: 20000 cycles = 208.33 µs */
    TIM2->SWEVGR = 1;            /* UG: latch PSC/ARR */
    TIM2->INTFR  = 0;            /* clear UIF */
    TIM2->CTLR1  = 0;            /* not running yet */
}

/* Block exactly one bit period (20000 cycles ≈ 208 µs at 96 MHz). */
static inline void wait_bit(void)
{
    TIM2->CNT   = 0;
    TIM2->INTFR = 0;
    TIM2->CTLR1 = 0x0001;               /* CEN = 1 */
    while (!(TIM2->INTFR & 0x0001));    /* wait UIF */
    TIM2->CTLR1 = 0;
}

/* --------------- Bit-bang TX on PA10 (4800 8N1) ----------- */
/* PA10 must be GPIO Out_PP before calling.                    */
static void bb_tx_byte(uint8_t byte)
{
    /* Start bit (LOW) */
    GPIOA->BCR = GPIO_Pin_10;
    wait_bit();

    /* 8 data bits, LSB first */
    for (int i = 0; i < 8; i++) {
        if (byte & (1u << i))
            GPIOA->BSHR = GPIO_Pin_10;
        else
            GPIOA->BCR  = GPIO_Pin_10;
        wait_bit();
    }

    /* Stop bit (HIGH) */
    GPIOA->BSHR = GPIO_Pin_10;
    wait_bit();

    /* Extra guard: one more idle bit period */
    wait_bit();
}

/* ---------------------------------------------------------- */

int main(void)
{
    GPIO_InitTypeDef  gpio  = {0};
    USART_InitTypeDef usart = {0};

    ael_mailbox_init();

    /* Clocks: GPIOA + USART1 on APB2 */
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_USART1, ENABLE);

    /* PA9: USART1 TX in half-duplex AF open-drain mode */
    gpio.GPIO_Pin   = GPIO_Pin_9;
    gpio.GPIO_Mode  = GPIO_Mode_AF_OD;
    gpio.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &gpio);

    /* PA10: IPU initially — weak pull-up for the open-drain PA9 line */
    gpio.GPIO_Pin  = GPIO_Pin_10;
    gpio.GPIO_Mode = GPIO_Mode_IPU;
    GPIO_Init(GPIOA, &gpio);

    /* USART1: 4800 baud 8N1, Tx+Rx, HDSEL */
    usart.USART_BaudRate            = 4800;
    usart.USART_WordLength          = USART_WordLength_8b;
    usart.USART_StopBits            = USART_StopBits_1;
    usart.USART_Parity              = USART_Parity_No;
    usart.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    usart.USART_Mode                = USART_Mode_Tx | USART_Mode_Rx;
    USART_Init(USART1, &usart);
    USART_HalfDuplexCmd(USART1, ENABLE);
    USART_Cmd(USART1, ENABLE);

    /* Flush any stale RXNE */
    if (USART1->STATR & USART_FLAG_RXNE) (void)USART1->DATAR;

    /* TIM2 for accurate bit timing */
    tim2_init();

    for (int i = 0; i < BUF_SIZE; i++) {
        /* ---- TX phase: PA10 = IPU (pull-up for HDSEL OD line) ---- */
        gpio.GPIO_Pin  = GPIO_Pin_10;
        gpio.GPIO_Mode = GPIO_Mode_IPU;
        GPIO_Init(GPIOA, &gpio);

        while (!(USART1->STATR & USART_FLAG_TXE));
        USART1->DATAR = TxBuf[i];
        while (!(USART1->STATR & USART_FLAG_TC));

        /* ---- Echo phase: PA10 = GPIO Out_PP, bit-bang echo ---- */
        /* Set HIGH before switching mode to avoid glitch */
        gpio.GPIO_Pin   = GPIO_Pin_10;
        gpio.GPIO_Mode  = GPIO_Mode_Out_PP;
        gpio.GPIO_Speed = GPIO_Speed_50MHz;
        GPIO_Init(GPIOA, &gpio);
        GPIOA->BSHR = GPIO_Pin_10;   /* idle HIGH */

        /* HDSEL mode: after TC the receiver needs ~1-2 bit periods to
         * fully re-activate.  Wait 3 bit periods (3×208µs=625µs) to
         * guarantee RX is ready before the echo start bit. */
        wait_bit(); wait_bit(); wait_bit();

        /* Clear any residual overrun before the echo */
        if (USART1->STATR & USART_FLAG_ORE) (void)USART1->DATAR;

        /* Send echo via PA10 → PA9 → USART1 RX */
        bb_tx_byte(TxBuf[i]);

        /* ---- Receive: wait for USART1 RXNE ---- */
        uint32_t t = 4000000;
        while (!(USART1->STATR & USART_FLAG_RXNE) && --t);
        if (!t) {
            uint32_t d0 = ((uint32_t)(USART1->STATR & 0xFFFF) << 16) | (uint32_t)i;
            ael_mailbox_fail(3, d0);
            while (1) { AEL_MAILBOX->detail0 = d0; }
        }
        uint8_t rx = (uint8_t)USART1->DATAR;
        if (rx != TxBuf[i]) {
            /* detail0: [31:24]=received, [23:16]=expected, [7:0]=index */
            ael_mailbox_fail(1, ((uint32_t)rx << 24) | ((uint32_t)TxBuf[i] << 16) | (uint32_t)i);
            while (1);
        }
    }

    ael_mailbox_pass();
    AEL_MAILBOX->detail0 = 0x600D;

    uint32_t tick = 0;
    while (1) {
        AEL_MAILBOX->detail0 = ++tick;
        for (volatile int i = 0; i < 960000; i++);
    }
}
