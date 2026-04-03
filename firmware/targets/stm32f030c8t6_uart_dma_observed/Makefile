CC      = arm-none-eabi-gcc
OBJCOPY = arm-none-eabi-objcopy

BUILD_DIR ?= build
OUT_ELF   ?= $(BUILD_DIR)/stm32f030c8t6_uart_dma_observed_app.elf
OUT_BIN   ?= $(BUILD_DIR)/stm32f030c8t6_uart_dma_observed_app.bin

BASE = ../stm32f030c8t6
HAL_BASE = ../stm32f030c8t6_uart_dma_hal_observed

CFLAGS  = -mcpu=cortex-m0 -mthumb \
          -O0 -g3 \
          -ffreestanding -fdata-sections -ffunction-sections \
          -Wall -Wextra \
          -DSTM32F030x8 \
          -I$(BASE) \
          -I$(HAL_BASE)/vendor/include/st \
          -I/tmp/STM32CubeF0/Drivers/CMSIS/Core/Include

LDFLAGS = -T $(BASE)/stm32f030.ld -Wl,--gc-sections -nostartfiles -nostdlib

OBJS = $(BUILD_DIR)/main.o $(BUILD_DIR)/startup.o $(BUILD_DIR)/system_stm32f0xx.o

all: $(OUT_ELF) $(OUT_BIN)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(BUILD_DIR)/main.o: main.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/startup.o: startup.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/system_stm32f0xx.o: $(HAL_BASE)/vendor/include/st/system_stm32f0xx.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OUT_ELF): $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) -o $@ $(LDFLAGS) -lgcc

$(OUT_BIN): $(OUT_ELF)
	$(OBJCOPY) -O binary $< $@

clean:
	rm -rf $(BUILD_DIR)
