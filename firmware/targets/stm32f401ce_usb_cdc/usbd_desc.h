#ifndef AEL_STM32F401CE_USB_DESC_H
#define AEL_STM32F401CE_USB_DESC_H

#include "usbd_def.h"

#define DEVICE_ID1 ((uint32_t)0x1FFF7A10U)
#define DEVICE_ID2 ((uint32_t)0x1FFF7A14U)
#define DEVICE_ID3 ((uint32_t)0x1FFF7A18U)

#define USB_SIZ_STRING_SERIAL 0x1A

extern USBD_DescriptorsTypeDef AEL_USB_Desc;

#endif
