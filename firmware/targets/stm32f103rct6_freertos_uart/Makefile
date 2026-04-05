CC      = arm-none-eabi-gcc
OBJCOPY = arm-none-eabi-objcopy

BUILD_DIR ?= build
OUT_ELF   ?= $(BUILD_DIR)/stm32f103rct6_freertos_uart.elf
OUT_BIN   ?= $(BUILD_DIR)/stm32f103rct6_freertos_uart.bin

BASE     = ../stm32f103rct6
FRTDIR   = ../../third_party/FreeRTOS-Kernel

CFLAGS  = -mcpu=cortex-m3 -mthumb \
          -O0 -g3 \
          -ffreestanding -fdata-sections -ffunction-sections \
          -Wall -Wextra \
          -I$(BASE) \
          -I$(FRTDIR)/include \
          -I$(FRTDIR)/portable/GCC/ARM_CM3 \
          -I.

LDFLAGS = -T $(BASE)/stm32f103rct6.ld \
          -Wl,--gc-sections -nostartfiles -nostdlib \
          -specs=nano.specs -lc

OBJS = \
    $(BUILD_DIR)/startup.o \
    $(BUILD_DIR)/main.o \
    $(BUILD_DIR)/frt_tasks.o \
    $(BUILD_DIR)/frt_list.o \
    $(BUILD_DIR)/frt_queue.o \
    $(BUILD_DIR)/frt_port.o \
    $(BUILD_DIR)/frt_heap4.o

all: $(OUT_ELF) $(OUT_BIN)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(BUILD_DIR)/startup.o: $(BASE)/startup_stm32f103.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/main.o: main.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/frt_tasks.o: $(FRTDIR)/tasks.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/frt_list.o: $(FRTDIR)/list.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/frt_queue.o: $(FRTDIR)/queue.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/frt_port.o: $(FRTDIR)/portable/GCC/ARM_CM3/port.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/frt_heap4.o: $(FRTDIR)/portable/MemMang/heap_4.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OUT_ELF): $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) -o $@ $(LDFLAGS) -lgcc

$(OUT_BIN): $(OUT_ELF)
	$(OBJCOPY) -O binary $< $@

clean:
	rm -rf $(BUILD_DIR)
