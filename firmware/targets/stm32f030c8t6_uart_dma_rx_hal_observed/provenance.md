# STM32F030C8T6 UART DMA RX HAL observed target provenance

This target reuses the vendored ST official F0 HAL and CMSIS device sources from:

- [stm32f030c8t6_uart_dma_hal_observed](/home/ali/work/ai-embedded-lab/firmware/targets/stm32f030c8t6_uart_dma_hal_observed)

Purpose:

- prove `USART1 RX DMA` on the current DAPLink UART fixture
- host sends a fixed frame on `DAPLink TX -> PA10`
- DUT receives with `HAL_UART_Receive_DMA()`
- DUT reports success over polling UART TX on `PA9 -> DAPLink RX`

This target is diagnostic and deferred, not part of the canonical suite.
