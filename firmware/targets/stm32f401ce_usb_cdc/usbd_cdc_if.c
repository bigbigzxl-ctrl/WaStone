#include "main.h"

#include <string.h>

#define APP_RX_DATA_SIZE  256U
#define APP_TX_DATA_SIZE  256U
#define APP_CMD_SIZE      96U

static uint8_t UserRxBuffer[APP_RX_DATA_SIZE];
static uint8_t UserTxBuffer[APP_TX_DATA_SIZE];
static char CommandBuffer[APP_CMD_SIZE];
static uint32_t CommandLen = 0;
static uint32_t LastHeartbeatTick = 0;
static uint32_t LastBannerTick = 0;
static uint8_t FirstReadySent = 0;

static int8_t CDC_Itf_Init(void);
static int8_t CDC_Itf_DeInit(void);
static int8_t CDC_Itf_Control(uint8_t cmd, uint8_t *pbuf, uint16_t length);
static int8_t CDC_Itf_Receive(uint8_t *pbuf, uint32_t *len);
static int8_t CDC_Itf_TransmitCplt(uint8_t *pbuf, uint32_t *len, uint8_t epnum);
static void APP_USB_CDC_HandleLine(const char *line);

USBD_CDC_ItfTypeDef AEL_USB_CDC_fops = {
    CDC_Itf_Init,
    CDC_Itf_DeInit,
    CDC_Itf_Control,
    CDC_Itf_Receive,
    CDC_Itf_TransmitCplt,
};

extern USBD_HandleTypeDef hUsbDeviceFS;

static int8_t CDC_Itf_Init(void)
{
    CommandLen = 0;
    LastHeartbeatTick = HAL_GetTick();
    LastBannerTick = HAL_GetTick();
    FirstReadySent = 0;
    USBD_CDC_SetTxBuffer(&hUsbDeviceFS, UserTxBuffer, 0);
    USBD_CDC_SetRxBuffer(&hUsbDeviceFS, UserRxBuffer);
    return (int8_t)USBD_OK;
}

static int8_t CDC_Itf_DeInit(void)
{
    return (int8_t)USBD_OK;
}

static int8_t CDC_Itf_Control(uint8_t cmd, uint8_t *pbuf, uint16_t length)
{
    UNUSED(length);
    switch (cmd) {
    case CDC_SET_LINE_CODING:
    case CDC_GET_LINE_CODING:
    case CDC_SET_CONTROL_LINE_STATE:
    case CDC_SEND_BREAK:
    default:
        break;
    }
    UNUSED(pbuf);
    return (int8_t)USBD_OK;
}

static int8_t CDC_Itf_Receive(uint8_t *pbuf, uint32_t *len)
{
    uint32_t count = (len != NULL) ? *len : 0U;
    for (uint32_t idx = 0; idx < count; idx++) {
        char ch = (char)pbuf[idx];
        if (ch == '\r') {
            continue;
        }
        if (ch == '\n') {
            CommandBuffer[CommandLen] = '\0';
            APP_USB_CDC_HandleLine(CommandBuffer);
            CommandLen = 0;
            continue;
        }
        if (CommandLen + 1U < APP_CMD_SIZE) {
            CommandBuffer[CommandLen++] = ch;
        } else {
            APP_USB_CDC_TransmitString("ERR\r\n");
            CommandLen = 0;
        }
    }

    USBD_CDC_SetRxBuffer(&hUsbDeviceFS, UserRxBuffer);
    USBD_CDC_ReceivePacket(&hUsbDeviceFS);
    return (int8_t)USBD_OK;
}

static int8_t CDC_Itf_TransmitCplt(uint8_t *pbuf, uint32_t *len, uint8_t epnum)
{
    UNUSED(pbuf);
    UNUSED(len);
    UNUSED(epnum);
    return (int8_t)USBD_OK;
}

uint8_t APP_USB_CDC_IsConfigured(void)
{
    return (uint8_t)(hUsbDeviceFS.dev_state == USBD_STATE_CONFIGURED);
}

uint8_t APP_USB_CDC_TransmitString(const char *text)
{
    size_t len = strlen(text);
    USBD_CDC_HandleTypeDef *hcdc;

    if (!APP_USB_CDC_IsConfigured()) {
        return (uint8_t)USBD_FAIL;
    }
    if (len == 0U || len >= APP_TX_DATA_SIZE) {
        return (uint8_t)USBD_FAIL;
    }

    hcdc = (USBD_CDC_HandleTypeDef *)hUsbDeviceFS.pClassData;
    if (hcdc == NULL || hcdc->TxState != 0U) {
        return (uint8_t)USBD_BUSY;
    }

    memcpy(UserTxBuffer, text, len);
    USBD_CDC_SetTxBuffer(&hUsbDeviceFS, UserTxBuffer, (uint32_t)len);
    return USBD_CDC_TransmitPacket(&hUsbDeviceFS);
}

void APP_USB_CDC_Process(void)
{
    uint32_t now = HAL_GetTick();

    if ((now - LastHeartbeatTick) >= 500U) {
        LastHeartbeatTick = now;
        GPIOC->ODR ^= GPIO_ODR_OD13;
    }

    if (!APP_USB_CDC_IsConfigured()) {
        FirstReadySent = 0;
        return;
    }

    if (!FirstReadySent) {
        if (APP_USB_CDC_TransmitString("AEL_USB_CDC_READY\r\n") == USBD_OK) {
            FirstReadySent = 1;
            LastBannerTick = now;
        }
        return;
    }

    if ((now - LastBannerTick) >= 2000U) {
        if (APP_USB_CDC_TransmitString("AEL_USB_READY\r\n") == USBD_OK) {
            LastBannerTick = now;
        }
    }
}

static void APP_USB_CDC_HandleLine(const char *line)
{
    if (strcmp(line, "PING") == 0) {
        APP_USB_CDC_TransmitString("PONG\r\n");
        return;
    }
    if (strncmp(line, "ECHO ", 5) == 0) {
        size_t text_len = strlen(line);
        if (text_len + 3U < APP_TX_DATA_SIZE) {
            memcpy(UserTxBuffer, line, text_len);
            UserTxBuffer[text_len] = '\r';
            UserTxBuffer[text_len + 1U] = '\n';
            UserTxBuffer[text_len + 2U] = '\0';
            APP_USB_CDC_TransmitString((const char *)UserTxBuffer);
            return;
        }
    }
    APP_USB_CDC_TransmitString("ERR\r\n");
}
