CC=arm-none-eabi-gcc
OBJCOPY=arm-none-eabi-objcopy

BUILD_DIR ?= build
OUT_ELF ?= $(BUILD_DIR)/stm32f401ce_usb_cdc_app.elf
OUT_BIN ?= $(BUILD_DIR)/stm32f401ce_usb_cdc_app.bin

CUBE_ROOT = ../../../third_party/cache/STM32CubeF4
CMSIS_CORE_INC = $(CUBE_ROOT)/Drivers/CMSIS/Include
HAL_INC = $(CUBE_ROOT)/Drivers/STM32F4xx_HAL_Driver/Inc
USB_CORE_INC = $(CUBE_ROOT)/Middlewares/ST/STM32_USB_Device_Library/Core/Inc
USB_CDC_INC = $(CUBE_ROOT)/Middlewares/ST/STM32_USB_Device_Library/Class/CDC/Inc
CMSIS_DEV_INC = ../stm32f401ce/vendor/include/st

CFLAGS = -mcpu=cortex-m4 -mthumb -mfpu=fpv4-sp-d16 -mfloat-abi=hard \
         -O2 -ffreestanding \
         -fdata-sections -ffunction-sections -Wall -Wextra -Wno-unused-parameter \
         -DSTM32F401xC -DUSE_HAL_DRIVER -DUSE_USB_FS \
         -I. -I$(CMSIS_CORE_INC) -I$(HAL_INC) -I$(USB_CORE_INC) -I$(USB_CDC_INC) -I$(CMSIS_DEV_INC)

LDFLAGS = -T ../stm32f401ce/stm32f401ce.ld -Wl,--gc-sections -nostartfiles -nostdlib

SRCS = \
	main.c \
	usbd_desc.c \
	usbd_conf.c \
	usbd_cdc_if.c \
	stm32f4xx_it.c \
	../stm32f401ce/vendor/st/system_stm32f4xx.c \
	$(CUBE_ROOT)/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal.c \
	$(CUBE_ROOT)/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_cortex.c \
	$(CUBE_ROOT)/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_flash.c \
	$(CUBE_ROOT)/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_flash_ex.c \
	$(CUBE_ROOT)/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_gpio.c \
	$(CUBE_ROOT)/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_pcd.c \
	$(CUBE_ROOT)/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_pcd_ex.c \
	$(CUBE_ROOT)/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_pwr.c \
	$(CUBE_ROOT)/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_pwr_ex.c \
	$(CUBE_ROOT)/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_rcc.c \
	$(CUBE_ROOT)/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_rcc_ex.c \
	$(CUBE_ROOT)/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_ll_usb.c \
	$(CUBE_ROOT)/Middlewares/ST/STM32_USB_Device_Library/Core/Src/usbd_core.c \
	$(CUBE_ROOT)/Middlewares/ST/STM32_USB_Device_Library/Core/Src/usbd_ctlreq.c \
	$(CUBE_ROOT)/Middlewares/ST/STM32_USB_Device_Library/Core/Src/usbd_ioreq.c \
	$(CUBE_ROOT)/Middlewares/ST/STM32_USB_Device_Library/Class/CDC/Src/usbd_cdc.c

ASMSRCS = ../stm32f401ce/vendor/st/startup_stm32f401xc.s
OBJS = $(SRCS:%.c=$(BUILD_DIR)/%.o) $(ASMSRCS:%.s=$(BUILD_DIR)/%.o)

all: $(OUT_ELF) $(OUT_BIN)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(BUILD_DIR)/%.o: %.c | $(BUILD_DIR)
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/%.o: %.s | $(BUILD_DIR)
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(OUT_ELF): $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) -o $@ $(LDFLAGS) -lc -lgcc

$(OUT_BIN): $(OUT_ELF)
	$(OBJCOPY) -O binary $< $@

clean:
	rm -rf $(BUILD_DIR)
