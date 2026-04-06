#include "usbd_core.h"
#include "usbd_desc.h"
#include "usbd_conf.h"

#define USBD_VID                      0x1209
#define USBD_PID                      0x4010
#define USBD_LANGID_STRING            0x0409
#define USBD_MANUFACTURER_STRING      "OpenAI AEL"
#define USBD_PRODUCT_FS_STRING        "STM32F401 USB CDC Test"
#define USBD_CONFIGURATION_FS_STRING  "CDC Config"
#define USBD_INTERFACE_FS_STRING      "CDC Interface"

static uint8_t *AEL_USB_DeviceDescriptor(USBD_SpeedTypeDef speed, uint16_t *length);
static uint8_t *AEL_USB_LangIDStrDescriptor(USBD_SpeedTypeDef speed, uint16_t *length);
static uint8_t *AEL_USB_ManufacturerStrDescriptor(USBD_SpeedTypeDef speed, uint16_t *length);
static uint8_t *AEL_USB_ProductStrDescriptor(USBD_SpeedTypeDef speed, uint16_t *length);
static uint8_t *AEL_USB_SerialStrDescriptor(USBD_SpeedTypeDef speed, uint16_t *length);
static uint8_t *AEL_USB_ConfigStrDescriptor(USBD_SpeedTypeDef speed, uint16_t *length);
static uint8_t *AEL_USB_InterfaceStrDescriptor(USBD_SpeedTypeDef speed, uint16_t *length);
static void IntToUnicode(uint32_t value, uint8_t *pbuf, uint8_t len);
static void GetSerialNum(void);

USBD_DescriptorsTypeDef AEL_USB_Desc = {
    AEL_USB_DeviceDescriptor,
    AEL_USB_LangIDStrDescriptor,
    AEL_USB_ManufacturerStrDescriptor,
    AEL_USB_ProductStrDescriptor,
    AEL_USB_SerialStrDescriptor,
    AEL_USB_ConfigStrDescriptor,
    AEL_USB_InterfaceStrDescriptor,
};

__ALIGN_BEGIN static uint8_t USBD_DeviceDesc[USB_LEN_DEV_DESC] __ALIGN_END = {
    0x12,
    USB_DESC_TYPE_DEVICE,
    0x00, 0x02,
    0x02,
    0x02,
    0x00,
    USB_MAX_EP0_SIZE,
    LOBYTE(USBD_VID), HIBYTE(USBD_VID),
    LOBYTE(USBD_PID), HIBYTE(USBD_PID),
    0x00, 0x01,
    USBD_IDX_MFC_STR,
    USBD_IDX_PRODUCT_STR,
    USBD_IDX_SERIAL_STR,
    USBD_MAX_NUM_CONFIGURATION,
};

__ALIGN_BEGIN static uint8_t USBD_LangIDDesc[USB_LEN_LANGID_STR_DESC] __ALIGN_END = {
    USB_LEN_LANGID_STR_DESC,
    USB_DESC_TYPE_STRING,
    LOBYTE(USBD_LANGID_STRING),
    HIBYTE(USBD_LANGID_STRING),
};

__ALIGN_BEGIN static uint8_t USBD_StringSerial[USB_SIZ_STRING_SERIAL] __ALIGN_END = {
    USB_SIZ_STRING_SERIAL,
    USB_DESC_TYPE_STRING,
};

__ALIGN_BEGIN static uint8_t USBD_StrDesc[USBD_MAX_STR_DESC_SIZ] __ALIGN_END;

static uint8_t *AEL_USB_DeviceDescriptor(USBD_SpeedTypeDef speed, uint16_t *length)
{
    UNUSED(speed);
    *length = sizeof(USBD_DeviceDesc);
    return USBD_DeviceDesc;
}

static uint8_t *AEL_USB_LangIDStrDescriptor(USBD_SpeedTypeDef speed, uint16_t *length)
{
    UNUSED(speed);
    *length = sizeof(USBD_LangIDDesc);
    return USBD_LangIDDesc;
}

static uint8_t *AEL_USB_ManufacturerStrDescriptor(USBD_SpeedTypeDef speed, uint16_t *length)
{
    UNUSED(speed);
    USBD_GetString((uint8_t *)USBD_MANUFACTURER_STRING, USBD_StrDesc, length);
    return USBD_StrDesc;
}

static uint8_t *AEL_USB_ProductStrDescriptor(USBD_SpeedTypeDef speed, uint16_t *length)
{
    UNUSED(speed);
    USBD_GetString((uint8_t *)USBD_PRODUCT_FS_STRING, USBD_StrDesc, length);
    return USBD_StrDesc;
}

static uint8_t *AEL_USB_SerialStrDescriptor(USBD_SpeedTypeDef speed, uint16_t *length)
{
    UNUSED(speed);
    *length = USB_SIZ_STRING_SERIAL;
    GetSerialNum();
    return USBD_StringSerial;
}

static uint8_t *AEL_USB_ConfigStrDescriptor(USBD_SpeedTypeDef speed, uint16_t *length)
{
    UNUSED(speed);
    USBD_GetString((uint8_t *)USBD_CONFIGURATION_FS_STRING, USBD_StrDesc, length);
    return USBD_StrDesc;
}

static uint8_t *AEL_USB_InterfaceStrDescriptor(USBD_SpeedTypeDef speed, uint16_t *length)
{
    UNUSED(speed);
    USBD_GetString((uint8_t *)USBD_INTERFACE_FS_STRING, USBD_StrDesc, length);
    return USBD_StrDesc;
}

static void GetSerialNum(void)
{
    uint32_t serial0 = *(uint32_t *)DEVICE_ID1;
    uint32_t serial1 = *(uint32_t *)DEVICE_ID2;
    uint32_t serial2 = *(uint32_t *)DEVICE_ID3;

    serial0 += serial2;
    if (serial0 != 0U) {
        IntToUnicode(serial0, &USBD_StringSerial[2], 8);
        IntToUnicode(serial1, &USBD_StringSerial[18], 4);
    }
}

static void IntToUnicode(uint32_t value, uint8_t *pbuf, uint8_t len)
{
    for (uint8_t idx = 0; idx < len; idx++) {
        uint8_t nibble = (uint8_t)(value >> 28);
        pbuf[2 * idx] = (uint8_t)(nibble < 0xA ? ('0' + nibble) : ('A' + nibble - 10));
        pbuf[(2 * idx) + 1] = 0;
        value <<= 4;
    }
}
