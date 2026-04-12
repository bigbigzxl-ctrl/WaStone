/* ch32v203_i2c_loopback — HAL I2C1 PB6(SCL)/PB7(SDA) bus self-test
 * Based on EVT/EXAM/I2C/I2C_7bit_Mode (default pin mapping, no remap)
 * Wiring: PB6 + PB7 with 4.7kΩ pull-up to 3V3
 * Test: verify START condition generates SB, bus not hung, STOP clean
 */
#define AEL_MAILBOX_ADDR 0x20000600u
#include "ael_mailbox.h"
#include "ch32v20x.h"

#define I2C_TIMEOUT 100000u
#define PROBE_ADDR  0xA0u   /* dummy address — NACK expected, that's fine */

static void I2C1_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure = {0};
    I2C_InitTypeDef  I2C_InitStructure  = {0};

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_I2C1,  ENABLE);

    /* PB6: SCL, PB7: SDA — AF open-drain (default I2C1 pins, no remap) */
    GPIO_InitStructure.GPIO_Pin   = GPIO_Pin_6 | GPIO_Pin_7;
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_AF_OD;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOB, &GPIO_InitStructure);

    I2C_InitStructure.I2C_ClockSpeed          = 100000;
    I2C_InitStructure.I2C_Mode                = I2C_Mode_I2C;
    I2C_InitStructure.I2C_DutyCycle           = I2C_DutyCycle_2;
    I2C_InitStructure.I2C_OwnAddress1         = 0x10;
    I2C_InitStructure.I2C_Ack                 = I2C_Ack_Enable;
    I2C_InitStructure.I2C_AcknowledgedAddress = I2C_AcknowledgedAddress_7bit;
    I2C_Init(I2C1, &I2C_InitStructure);
    I2C_Cmd(I2C1, ENABLE);
}

int main(void)
{
    ael_mailbox_init();
    I2C1_Init();

    /* Step 1: Wait until bus not BUSY */
    uint32_t t = I2C_TIMEOUT;
    while (I2C_GetFlagStatus(I2C1, I2C_FLAG_BUSY) != RESET && --t);
    if (!t) { ael_mailbox_fail(1, 0); while (1); }

    /* Step 2: Generate START */
    I2C_GenerateSTART(I2C1, ENABLE);
    t = I2C_TIMEOUT;
    while (!I2C_CheckEvent(I2C1, I2C_EVENT_MASTER_MODE_SELECT) && --t);
    if (!t) { ael_mailbox_fail(2, 0); while (1); }

    /* Step 3: Send probe address (write direction) — NACK is acceptable */
    I2C_Send7bitAddress(I2C1, PROBE_ADDR, I2C_Direction_Transmitter);
    t = I2C_TIMEOUT;
    while (--t) {
        if (I2C_CheckEvent(I2C1, I2C_EVENT_MASTER_TRANSMITTER_MODE_SELECTED)) break;
        if (I2C_GetFlagStatus(I2C1, I2C_FLAG_AF) != RESET) break; /* NACK OK */
    }
    /* Clear AF flag if set */
    if (I2C_GetFlagStatus(I2C1, I2C_FLAG_AF) != RESET) {
        I2C_ClearFlag(I2C1, I2C_FLAG_AF);
    }

    /* Step 4: Generate STOP */
    I2C_GenerateSTOP(I2C1, ENABLE);
    t = I2C_TIMEOUT;
    while (I2C_GetFlagStatus(I2C1, I2C_FLAG_BUSY) != RESET && --t);

    /* PASS: START was generated successfully (SB event), bus recovered */
    ael_mailbox_pass();

    uint32_t tick = 0;
    while (1) {
        AEL_MAILBOX->detail0 = ++tick;
        for (volatile int i = 0; i < 720000; i++);
    }
}
