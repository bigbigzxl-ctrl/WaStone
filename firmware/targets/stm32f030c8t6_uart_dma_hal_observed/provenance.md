# STM32F030C8T6 UART DMA HAL observed target provenance

This target vendors the minimum ST official sources needed to prove
`USART1 TX DMA` on the current DAPLink UART fixture.

Upstream sources:
- HAL repo: `https://github.com/STMicroelectronics/stm32f0xx-hal-driver.git`
- CMSIS device repo: `https://github.com/STMicroelectronics/cmsis-device-f0.git`
- local cache date: `2026-04-03`

Copied ST files:
- `stm32f0xx-hal-driver/Inc/stm32f0xx_hal*.h`
  -> `vendor/hal/Inc/`
- `stm32f0xx-hal-driver/Src/stm32f0xx_hal.c`
  -> `vendor/hal/Src/stm32f0xx_hal.c`
- `stm32f0xx-hal-driver/Src/stm32f0xx_hal_cortex.c`
  -> `vendor/hal/Src/stm32f0xx_hal_cortex.c`
- `stm32f0xx-hal-driver/Src/stm32f0xx_hal_dma.c`
  -> `vendor/hal/Src/stm32f0xx_hal_dma.c`
- `stm32f0xx-hal-driver/Src/stm32f0xx_hal_gpio.c`
  -> `vendor/hal/Src/stm32f0xx_hal_gpio.c`
- `stm32f0xx-hal-driver/Src/stm32f0xx_hal_rcc.c`
  -> `vendor/hal/Src/stm32f0xx_hal_rcc.c`
- `stm32f0xx-hal-driver/Src/stm32f0xx_hal_rcc_ex.c`
  -> `vendor/hal/Src/stm32f0xx_hal_rcc_ex.c`
- `stm32f0xx-hal-driver/Src/stm32f0xx_hal_uart.c`
  -> `vendor/hal/Src/stm32f0xx_hal_uart.c`
- `cmsis-device-f0/Include/stm32f0xx.h`
  -> `vendor/include/st/stm32f0xx.h`
- `cmsis-device-f0/Include/stm32f030x8.h`
  -> `vendor/include/st/stm32f030x8.h`
- `cmsis-device-f0/Include/system_stm32f0xx.h`
  -> `vendor/include/st/system_stm32f0xx.h`
- `cmsis-device-f0/Source/Templates/system_stm32f0xx.c`
  -> `vendor/include/st/system_stm32f0xx.c`

AEL-owned files:
- `main.c`
- `startup.c`
- `stm32f0xx_hal_conf.h`
- `Makefile`
- `provenance.md`

Local AEL decisions:
- the target is intentionally `TX-only observed`, not a suite member
- baud is `9600`, matching the ST reference example style
- GPIOA `PA9/PA10` use pull-up + high-speed AF configuration
- `SysTick_Handler` is provided locally so `HAL_GetTick()` advances without a
  larger Cube project scaffold
