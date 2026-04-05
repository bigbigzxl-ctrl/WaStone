CC      = arm-none-eabi-gcc
OBJCOPY = arm-none-eabi-objcopy

BUILD_DIR ?= build
OUT_ELF   ?= $(BUILD_DIR)/stm32f407_freertos_uart.elf
OUT_BIN   ?= $(BUILD_DIR)/stm32f407_freertos_uart.bin

BASE     = ../stm32f407_discovery
FRTDIR   = ../../third_party/FreeRTOS-Kernel
PORT     = $(FRTDIR)/portable/GCC/ARM_CM3

CUBE_ROOT      = ../../../third_party/cache/STM32CubeF4
CMSIS_CORE_INC = $(CUBE_ROOT)/Drivers/CMSIS/Include
CMSIS_DEV_INC  = $(BASE)/vendor/include/st

# CM4 core, no FPU (ARM_CM3 port does not save FPU context)
CFLAGS  = -mcpu=cortex-m4 -mthumb -mfloat-abi=soft \
          -O0 -g3 \
          -ffreestanding -fdata-sections -ffunction-sections \
          -Wall -Wextra \
          -DSTM32F407xx \
          -I$(CMSIS_CORE_INC) \
          -I$(CMSIS_DEV_INC) \
          -I$(BASE) \
          -I$(FRTDIR)/include \
          -I$(PORT) \
          -I.

LDFLAGS = -T $(BASE)/stm32f407.ld \
          -Wl,--gc-sections -nostartfiles -nostdlib \
          -specs=nano.specs -lc

OBJS = \
    $(BUILD_DIR)/startup.o \
    $(BUILD_DIR)/system.o \
    $(BUILD_DIR)/main.o \
    $(BUILD_DIR)/frt_tasks.o \
    $(BUILD_DIR)/frt_list.o \
    $(BUILD_DIR)/frt_queue.o \
    $(BUILD_DIR)/frt_port.o \
    $(BUILD_DIR)/frt_heap4.o

all: $(OUT_ELF) $(OUT_BIN)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(BUILD_DIR)/startup.o: $(BASE)/vendor/st/startup_stm32f407xx.s | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/system.o: $(BASE)/vendor/st/system_stm32f4xx.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/main.o: main.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/frt_tasks.o: $(FRTDIR)/tasks.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/frt_list.o: $(FRTDIR)/list.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/frt_queue.o: $(FRTDIR)/queue.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/frt_port.o: $(PORT)/port.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/frt_heap4.o: $(FRTDIR)/portable/MemMang/heap_4.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OUT_ELF): $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) -o $@ $(LDFLAGS) -lgcc

$(OUT_BIN): $(OUT_ELF)
	$(OBJCOPY) -O binary $< $@

clean:
	rm -rf $(BUILD_DIR)
