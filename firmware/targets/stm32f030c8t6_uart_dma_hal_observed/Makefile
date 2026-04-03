CC=arm-none-eabi-gcc
OBJCOPY=arm-none-eabi-objcopy

BUILD_DIR?=build
OUT_ELF?=$(BUILD_DIR)/stm32f030c8t6_uart_dma_hal_observed_app.elf
OUT_BIN?=$(BUILD_DIR)/stm32f030c8t6_uart_dma_hal_observed_app.bin

BASE=../stm32f030c8t6
HAL=vendor/hal
DEV=vendor/include/st
CMSIS=/tmp/STM32CubeF0/Drivers/CMSIS/Core/Include

CFLAGS=-mcpu=cortex-m0 -mthumb -O0 -g3 -ffreestanding -fdata-sections -ffunction-sections -Wall -Wextra \
       -DSTM32F030x8 -DUSE_HAL_DRIVER \
       -I. -I$(HAL)/Inc -I$(DEV) -I$(CMSIS)
LDFLAGS=-T $(BASE)/stm32f030.ld -Wl,--gc-sections -nostartfiles -nostdlib

OBJS=$(BUILD_DIR)/main.o \
     $(BUILD_DIR)/startup.o \
     $(BUILD_DIR)/stm32f0xx_hal.o \
     $(BUILD_DIR)/stm32f0xx_hal_cortex.o \
     $(BUILD_DIR)/stm32f0xx_hal_dma.o \
     $(BUILD_DIR)/stm32f0xx_hal_gpio.o \
     $(BUILD_DIR)/stm32f0xx_hal_rcc.o \
     $(BUILD_DIR)/stm32f0xx_hal_rcc_ex.o \
     $(BUILD_DIR)/stm32f0xx_hal_uart.o \
     $(BUILD_DIR)/system_stm32f0xx.o

all: $(OUT_ELF) $(OUT_BIN)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(BUILD_DIR)/main.o: main.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/startup.o: startup.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/stm32f0xx_hal.o: $(HAL)/Src/stm32f0xx_hal.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/stm32f0xx_hal_cortex.o: $(HAL)/Src/stm32f0xx_hal_cortex.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/stm32f0xx_hal_dma.o: $(HAL)/Src/stm32f0xx_hal_dma.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/stm32f0xx_hal_gpio.o: $(HAL)/Src/stm32f0xx_hal_gpio.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/stm32f0xx_hal_rcc.o: $(HAL)/Src/stm32f0xx_hal_rcc.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/stm32f0xx_hal_rcc_ex.o: $(HAL)/Src/stm32f0xx_hal_rcc_ex.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/stm32f0xx_hal_uart.o: $(HAL)/Src/stm32f0xx_hal_uart.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/system_stm32f0xx.o: $(DEV)/system_stm32f0xx.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OUT_ELF): $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) -o $@ $(LDFLAGS) -lgcc

$(OUT_BIN): $(OUT_ELF)
	$(OBJCOPY) -O binary $< $@

clean:
	rm -rf $(BUILD_DIR)
