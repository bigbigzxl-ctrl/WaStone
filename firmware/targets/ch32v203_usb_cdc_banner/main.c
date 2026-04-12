/* ch32v203_usb_cdc_banner — USB CDC (SimulateCDC USBLIB stack) banner test
 *
 * System clock: 96MHz (system_ch32v20x.c patched to 96MHz).
 * USB clock: 96MHz / 2 = 48MHz via RCC_USBCLKSource_PLLCLK_Div2.
 *
 * Init sequence: Set_USBConfig() → USB_Init() → USB_Interrupts_Config()
 * Wait for bDeviceState == CONFIGURED (USB enumeration by host), timeout 5s.
 * On enumeration: send ASCII banner "AEL_CH32V203_USB_CDC\r\n" via EP3.
 * Write mailbox PASS immediately after enumeration check.
 *
 * Requires: USB-C cable from board second USB port (PA11=D-, PA12=D+) to host.
 * detail0 on PASS: 0xCDC (encoded as 0x00000CDC).
 */
#define AEL_MAILBOX_ADDR 0x20000600u
#include "ael_mailbox.h"
#include "ch32v20x.h"
#include "usb_lib.h"
#include "usb_pwr.h"
#include "hw_config.h"

extern uint8_t USBD_Endp3_Busy;
uint8_t USBD_ENDPx_DataUp(uint8_t endp, uint8_t *pbuf, uint16_t len);

static const char banner[] = "AEL_CH32V203_USB_CDC\r\n";

int main(void)
{
    ael_mailbox_init();

    /* USB stack init (hw_config.c: Set_USBConfig calls RCC_USBCLKConfig) */
    Set_USBConfig();
    USB_Init();
    USB_Interrupts_Config();

    /* Wait for USB enumeration (bDeviceState == CONFIGURED = 4) */
    /* Timeout ~5 seconds at 96MHz: 96M * 5 / 6 iterations ≈ 80M */
    uint32_t t = 80000000u;
    while (bDeviceState != CONFIGURED && --t);

    if (!t) {
        /* Not enumerated — host not connected or bus issue */
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
    if (send_t) {
        USBD_ENDPx_DataUp(ENDP3, (uint8_t *)banner,
                          sizeof(banner) - 1);  /* -1: no null terminator */
    }

    ael_mailbox_pass();
    AEL_MAILBOX->detail0 = 0xCDC;

    uint32_t tick = 0;
    while (1) {
        AEL_MAILBOX->detail0 = ++tick;
        for (volatile int i = 0; i < 960000; i++);
    }
}
