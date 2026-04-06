#ifndef AEL_STM32F401CE_USBD_CDC_IF_H
#define AEL_STM32F401CE_USBD_CDC_IF_H

#include "usbd_cdc.h"

extern USBD_CDC_ItfTypeDef AEL_USB_CDC_fops;

uint8_t APP_USB_CDC_IsConfigured(void);
uint8_t APP_USB_CDC_TransmitString(const char *text);
void APP_USB_CDC_Process(void);

#endif
