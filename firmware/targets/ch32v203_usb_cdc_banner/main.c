/* ch32v203_usb_cdc_banner — USB CDC (SimulateCDC USBLIB stack) banner test
 *
 * System clock: 96MHz (system_ch32v20x.c patched to 96MHz).
 * USB clock: 96MHz / 2 = 48MHz via RCC_USBCLKSource_PLLCLK_Div2.
 *
 * Procedure:
 *   1. AEL flashes this firmware via WCHLink (SWD).
 *   2. After flash, if USB-C was already plugged in: unplug then replug.
 *      If USB-C was not plugged in: plug it in.
 *   3. Host enumerates the device (< 2s after plug).
 *   4. Firmware sends banner "AEL_CH32V203_USB_CDC\r\n" via EP3, writes PASS.
 *
 * Timeout: ~60s from firmware start. detail0=0xCDC on PASS.
 */
#define AEL_MAILBOX_ADDR 0x20000600u
#include "ael_mailbox.h"
#include "ch32v20x.h"
#include "debug.h"
#include "usb_lib.h"
#include "usb_pwr.h"
#include "hw_config.h"

extern uint8_t USBD_Endp3_Busy;
uint8_t USBD_ENDPx_DataUp(uint8_t endp, uint8_t *pbuf, uint16_t len);

static const char banner[] = "AEL_CH32V203_USB_CDC\r\n";

int main(void)
{
    ael_mailbox_init();

    /* Required by USB stack internals (Delay_Ms inside USBD_init) */
    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_1);
    SystemCoreClockUpdate();
    Delay_Init();

    /* Standard USB stack init */
    Set_USBConfig();
    USB_Init();
    USB_Interrupts_Config();

    /* Wait up to ~60s for host enumeration.
     * If device does not enumerate: unplug USB-C cable and replug.
     * Host enumerates in < 2s after plug. */
    uint32_t t = 1920000000u;   /* ~60s at 96MHz, 3-4 cycles per iteration */
    while (bDeviceState != CONFIGURED && --t);

    if (!t) {
        ael_mailbox_fail(1, bDeviceState);
        while (1) {
            AEL_MAILBOX->detail0 = bDeviceState;
            for (volatile int i = 0; i < 960000; i++);
        }
    }

    /* Short settle after enumeration */
    for (volatile uint32_t i = 0; i < 960000; i++);

    /* Send banner via EP3 IN */
    uint32_t send_t = 1000000;
    while (USBD_Endp3_Busy && --send_t);
    if (send_t)
        USBD_ENDPx_DataUp(ENDP3, (uint8_t *)banner, sizeof(banner) - 1);

    ael_mailbox_pass();
    AEL_MAILBOX->detail0 = 0xCDC;

    uint32_t tick = 0;
    while (1) {
        AEL_MAILBOX->detail0 = ++tick;
        for (volatile int i = 0; i < 960000; i++);
    }
}
