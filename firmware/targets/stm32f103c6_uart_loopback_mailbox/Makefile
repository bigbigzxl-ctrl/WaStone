CC=arm-none-eabi-gcc
OBJCOPY=arm-none-eabi-objcopy

BUILD_DIR ?= build
OUT_ELF ?= $(BUILD_DIR)/stm32f103c6_uart_loopback_mailbox_app.elf
OUT_BIN ?= $(BUILD_DIR)/stm32f103c6_uart_loopback_mailbox_app.bin

BASE = ../stm32f103c6

CFLAGS = -mcpu=cortex-m3 -mthumb -O2 -ffreestanding -fdata-sections -ffunction-sections -Wall -Wextra -nostdlib
LDFLAGS = -T $(BASE)/stm32f103c6.ld -Wl,--gc-sections -nostdlib

OBJS = $(BUILD_DIR)/startup_stm32f103.o $(BUILD_DIR)/main.o

all: $(OUT_ELF) $(OUT_BIN)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(BUILD_DIR)/startup_stm32f103.o: $(BASE)/startup_stm32f103.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/main.o: main.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OUT_ELF): $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) -o $@ $(LDFLAGS)

$(OUT_BIN): $(OUT_ELF)
	$(OBJCOPY) -O binary $< $@

clean:
	rm -rf $(BUILD_DIR)
