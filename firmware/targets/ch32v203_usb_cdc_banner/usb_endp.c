/* usb_endp.c — minimal endpoint callbacks for banner-only CDC test
 * No UART bridging; EP3 TX only.
 */
#include "usb_lib.h"
#include "usb_desc.h"
#include "usb_mem.h"
#include "hw_config.h"
#include "usb_istr.h"
#include "usb_pwr.h"
#include "usb_prop.h"

uint8_t USBD_Endp3_Busy;

void EP1_IN_Callback(void)  { }
void EP2_OUT_Callback(void) { SetEPRxValid(ENDP2); }

void EP3_IN_Callback(void)
{
    USBD_Endp3_Busy = 0;
}

uint8_t USBD_ENDPx_DataUp(uint8_t endp, uint8_t *pbuf, uint16_t len)
{
    if (endp == ENDP3) {
        if (USBD_Endp3_Busy) return USB_ERROR;
        USB_SIL_Write(EP3_IN, pbuf, len);
        USBD_Endp3_Busy = 1;
        SetEPTxStatus(ENDP3, EP_TX_VALID);
        return USB_SUCCESS;
    }
    return USB_ERROR;
}
