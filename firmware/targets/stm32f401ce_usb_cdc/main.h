#ifndef AEL_STM32F401CE_USB_CDC_MAIN_H
#define AEL_STM32F401CE_USB_CDC_MAIN_H

#include "stm32f4xx_hal.h"
#include "usbd_core.h"
#include "usbd_cdc.h"
#include "usbd_desc.h"
#include "usbd_cdc_if.h"

extern USBD_HandleTypeDef hUsbDeviceFS;

void Error_Handler(void);

#endif
