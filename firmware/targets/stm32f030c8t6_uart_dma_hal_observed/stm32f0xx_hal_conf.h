#ifndef __STM32F0xx_HAL_CONF_H
#define __STM32F0xx_HAL_CONF_H

#ifdef __cplusplus
extern "C" {
#endif

#define HAL_MODULE_ENABLED
#define HAL_CORTEX_MODULE_ENABLED
#define HAL_DMA_MODULE_ENABLED
#define HAL_FLASH_MODULE_ENABLED
#define HAL_GPIO_MODULE_ENABLED
#define HAL_RCC_MODULE_ENABLED
#define HAL_UART_MODULE_ENABLED

#define HSE_VALUE            8000000U
#define HSE_STARTUP_TIMEOUT  100U
#define HSI_VALUE            8000000U
#define HSI_STARTUP_TIMEOUT  5000U
#define LSI_VALUE            32000U
#define LSE_VALUE            32768U
#define LSE_STARTUP_TIMEOUT  5000U
#define HSI14_VALUE          14000000U
#define HSI48_VALUE          48000000U

#define VDD_VALUE                 3300U
#define TICK_INT_PRIORITY         ((uint32_t)(1U<<__NVIC_PRIO_BITS) - 1U)
#define USE_RTOS                  0U
#define PREFETCH_ENABLE           1U
#define INSTRUCTION_CACHE_ENABLE  0U
#define DATA_CACHE_ENABLE         0U
#define USE_SPI_CRC               1U

#define USE_HAL_UART_REGISTER_CALLBACKS 0U

#include "stm32f0xx_hal_rcc.h"
#include "stm32f0xx_hal_flash.h"
#include "stm32f0xx_hal_gpio.h"
#include "stm32f0xx_hal_dma.h"
#include "stm32f0xx_hal_cortex.h"
#include "stm32f0xx_hal_uart.h"

#ifdef USE_FULL_ASSERT
void assert_failed(uint8_t *file, uint32_t line);
#else
#define assert_param(expr) ((void)0U)
#endif

#ifdef __cplusplus
}
#endif

#endif
