#include <stdint.h>
#include "stm32f0xx_hal.h"

UART_HandleTypeDef huart1;
DMA_HandleTypeDef hdma_rx;

static volatile uint32_t g_rx_done;
static volatile uint32_t g_rx_error;

static uint8_t rx_buf[16];
static const uint8_t expected[] = "PING_DMA_RX_HAL";

static uint16_t cstr_len(const char *s)
{
    uint16_t n = 0u;
    while (s[n] != '\0') {
        ++n;
    }
    return n;
}

static void uart_poll_write(const char *s)
{
    (void)HAL_UART_Transmit(&huart1, (uint8_t *)s, cstr_len(s), 1000);
}

static void uart_hex32(uint32_t v)
{
    static const char hex[] = "0123456789ABCDEF";
    char out[8];
    int i;
    for (i = 0; i < 8; ++i) {
        out[7 - i] = hex[v & 0xFu];
        v >>= 4;
    }
    (void)HAL_UART_Transmit(&huart1, (uint8_t *)out, 8, 1000);
}

static int rx_matches_expected(void)
{
    uint32_t i;
    for (i = 0u; i < (sizeof(expected) - 1u); ++i) {
        if (rx_buf[i] != expected[i]) {
            return 0;
        }
    }
    return 1;
}

void HAL_MspInit(void)
{
}

void HAL_UART_MspInit(UART_HandleTypeDef *huart)
{
    GPIO_InitTypeDef gpio;

    if (huart->Instance != USART1) {
        return;
    }

    gpio.Pin = 0u;
    gpio.Mode = 0u;
    gpio.Pull = 0u;
    gpio.Speed = 0u;
    gpio.Alternate = 0u;

    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_DMA1_CLK_ENABLE();
    __HAL_RCC_USART1_CLK_ENABLE();

    gpio.Pin = GPIO_PIN_9 | GPIO_PIN_10;
    gpio.Mode = GPIO_MODE_AF_PP;
    gpio.Pull = GPIO_PULLUP;
    gpio.Speed = GPIO_SPEED_FREQ_HIGH;
    gpio.Alternate = GPIO_AF1_USART1;
    HAL_GPIO_Init(GPIOA, &gpio);

    hdma_rx.Instance = DMA1_Channel3;
    hdma_rx.Init.Direction = DMA_PERIPH_TO_MEMORY;
    hdma_rx.Init.PeriphInc = DMA_PINC_DISABLE;
    hdma_rx.Init.MemInc = DMA_MINC_ENABLE;
    hdma_rx.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
    hdma_rx.Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;
    hdma_rx.Init.Mode = DMA_NORMAL;
    hdma_rx.Init.Priority = DMA_PRIORITY_HIGH;
    HAL_DMA_Init(&hdma_rx);
    __HAL_LINKDMA(huart, hdmarx, hdma_rx);

    HAL_NVIC_SetPriority(DMA1_Channel2_3_IRQn, 0, 0);
    HAL_NVIC_EnableIRQ(DMA1_Channel2_3_IRQn);
    HAL_NVIC_SetPriority(USART1_IRQn, 0, 1);
    HAL_NVIC_EnableIRQ(USART1_IRQn);
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART1) {
        g_rx_done = 1u;
    }
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART1) {
        g_rx_error = huart->ErrorCode;
    }
}

void DMA1_Channel2_3_IRQHandler(void)
{
    HAL_DMA_IRQHandler(&hdma_rx);
}

void USART1_IRQHandler(void)
{
    HAL_UART_IRQHandler(&huart1);
}

void SysTick_Handler(void)
{
    HAL_IncTick();
}

int main(void)
{
    uint32_t start;
    uint32_t last_ready;
    uint32_t i;

    HAL_Init();

    huart1.Instance = USART1;
    huart1.Init.BaudRate = 9600;
    huart1.Init.WordLength = UART_WORDLENGTH_8B;
    huart1.Init.StopBits = UART_STOPBITS_1;
    huart1.Init.Parity = UART_PARITY_NONE;
    huart1.Init.Mode = UART_MODE_TX_RX;
    huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    huart1.Init.OverSampling = UART_OVERSAMPLING_16;
    huart1.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;

    if (HAL_UART_Init(&huart1) != HAL_OK) {
        while (1) {
        }
    }

    for (i = 0u; i < sizeof(rx_buf); ++i) {
        rx_buf[i] = 0u;
    }
    g_rx_done = 0u;
    g_rx_error = 0u;

    __HAL_UART_CLEAR_FLAG(&huart1, UART_CLEAR_OREF | UART_CLEAR_NEF | UART_CLEAR_PEF | UART_CLEAR_FEF);

    if (HAL_UART_Receive_DMA(&huart1, rx_buf, (uint16_t)(sizeof(expected) - 1u)) != HAL_OK) {
        uart_poll_write("AEL_HAL_UART_DMA_RX_START_FAIL err=");
        uart_hex32(huart1.ErrorCode);
        uart_poll_write("\r\n");
        while (1) {
        }
    }

    start = HAL_GetTick();
    last_ready = start - 500u;
    while ((g_rx_done == 0u) && (g_rx_error == 0u)) {
        if ((HAL_GetTick() - last_ready) >= 500u) {
            uart_poll_write("AEL_HAL_UART_DMA_RX_READY\r\n");
            last_ready = HAL_GetTick();
        }
        if ((HAL_GetTick() - start) > 4000u) {
            uart_poll_write("AEL_HAL_UART_DMA_RX_TIMEOUT dma_isr=");
            uart_hex32(DMA1->ISR);
            uart_poll_write(" cndtr=");
            uart_hex32(hdma_rx.Instance->CNDTR);
            uart_poll_write(" ccr=");
            uart_hex32(hdma_rx.Instance->CCR);
            uart_poll_write(" usart_isr=");
            uart_hex32(USART1->ISR);
            uart_poll_write("\r\n");
            break;
        }
    }

    if (g_rx_done != 0u) {
        if (rx_matches_expected()) {
            uart_poll_write("AEL_HAL_UART_DMA_RX_OK\r\n");
        } else {
            uart_poll_write("AEL_HAL_UART_DMA_RX_BAD rx0=");
            uart_hex32(rx_buf[0]);
            uart_poll_write(" rx1=");
            uart_hex32(rx_buf[1]);
            uart_poll_write(" rx2=");
            uart_hex32(rx_buf[2]);
            uart_poll_write(" rx3=");
            uart_hex32(rx_buf[3]);
            uart_poll_write("\r\n");
        }
    } else if (g_rx_error != 0u) {
        uart_poll_write("AEL_HAL_UART_DMA_RX_ERR code=");
        uart_hex32(g_rx_error);
        uart_poll_write(" dma_isr=");
        uart_hex32(DMA1->ISR);
        uart_poll_write("\r\n");
    }

    while (1) {
    }
}
