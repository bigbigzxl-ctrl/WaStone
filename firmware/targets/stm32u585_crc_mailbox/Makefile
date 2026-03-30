CC      = arm-none-eabi-gcc
OBJCOPY = arm-none-eabi-objcopy

BUILD_DIR ?= build
OUT_ELF   ?= $(BUILD_DIR)/stm32u585_crc_mailbox_app.elf
OUT_BIN   ?= $(BUILD_DIR)/stm32u585_crc_mailbox_app.bin

STARTUP_SRC = ../stm32u585ciu6/startup.c
LDSCRIPT    = ../stm32u585ciu6/stm32u585.ld

CFLAGS  = -mcpu=cortex-m33 -mthumb -O2 -ffreestanding -fdata-sections -ffunction-sections -Wall -Wextra -nostdlib
LDFLAGS = -T $(LDSCRIPT) -Wl,--gc-sections -nostdlib

OBJS = $(BUILD_DIR)/main.o $(BUILD_DIR)/startup.o

.PHONY: all clean

all: $(OUT_ELF) $(OUT_BIN)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(BUILD_DIR)/main.o: main.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/startup.o: $(STARTUP_SRC) | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OUT_ELF): $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) -o $@ $(LDFLAGS)

$(OUT_BIN): $(OUT_ELF)
	$(OBJCOPY) -O binary $< $@

clean:
	rm -rf $(BUILD_DIR)
