TARGET = stm32f030c8t6_uart_dma_rx_observed_app
BUILD_DIR ?= $(abspath $(CURDIR)/../../../artifacts/build_stm32f030c8t6_uart_dma_rx_observed)

CC = arm-none-eabi-gcc
OBJCOPY = arm-none-eabi-objcopy

CFLAGS = -mcpu=cortex-m0 -mthumb -O0 -g3 -ffreestanding -fdata-sections -ffunction-sections -Wall -Wextra
LDFLAGS = -T ../stm32f030c8t6/stm32f030.ld -Wl,--gc-sections -nostartfiles -nostdlib -lgcc

INCLUDES = -I../stm32f030c8t6

SRCS = main.c ../stm32f030c8t6/startup.c
OBJS = $(addprefix $(BUILD_DIR)/,$(notdir $(SRCS:.c=.o)))

all: $(BUILD_DIR)/$(TARGET).elf $(BUILD_DIR)/$(TARGET).bin

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(BUILD_DIR)/%.o: %.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

$(BUILD_DIR)/startup.o: ../stm32f030c8t6/startup.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

$(BUILD_DIR)/$(TARGET).elf: $(OBJS)
	$(CC) $(CFLAGS) $(INCLUDES) $^ -o $@ $(LDFLAGS)

$(BUILD_DIR)/$(TARGET).bin: $(BUILD_DIR)/$(TARGET).elf
	$(OBJCOPY) -O binary $< $@

clean:
	rm -rf $(BUILD_DIR)

.PHONY: all clean
