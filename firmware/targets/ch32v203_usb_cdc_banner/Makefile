TARGET   = ch32v203_usb_cdc_banner
TOOLCHAIN = /nvme1t/wch-riscv-gcc/bin
GCC      = $(TOOLCHAIN)/riscv-none-embed-gcc
OBJCOPY  = $(TOOLCHAIN)/riscv-none-embed-objcopy
OUT_ELF ?= $(TARGET).elf
OUT_BIN ?= $(TARGET).bin
ARCH_FLAGS = -march=rv32imac -mabi=ilp32

HAL_DIR   = /nvme1t/github/ch32v20x/EVT/EXAM/SRC/Peripheral/src
USB_EVT   = /nvme1t/github/ch32v20x/EVT/EXAM/USB/USBD/SimulateCDC/User
USB_CFG   = $(USB_EVT)/USBLIB/CONFIG
USB_DRV   = $(USB_EVT)/USBLIB/USB-Driver/src
USB_INC   = $(USB_EVT)/USBLIB/USB-Driver/inc

# Local dir first so usb_endp.c overrides the UART-dependent EVT version
VPATH = .:$(HAL_DIR):$(USB_CFG):$(USB_DRV)

CFLAGS = $(ARCH_FLAGS) -O2 -g -nostdlib -ffunction-sections -fdata-sections \
         -I. -I$(USB_CFG) -I$(USB_INC) -I$(USB_DRV) -Wall

LDFLAGS = $(ARCH_FLAGS) -T Link.ld -nostdlib -Wl,--gc-sections -Wl,-Map=$(TARGET).map

# HAL sources
HAL_SRCS = ch32v20x_rcc.c ch32v20x_gpio.c ch32v20x_usart.c ch32v20x_misc.c \
           ch32v20x_exti.c

# USBLIB CONFIG (usb_endp.c comes from local dir, not USB_CFG)
USB_CFG_SRCS = hw_config.c usb_desc.c usb_istr.c usb_prop.c usb_pwr.c

# USB Driver sources
USB_DRV_SRCS = usb_core.c usb_init.c usb_int.c usb_mem.c usb_regs.c usb_sil.c

SRCS = startup_ch32v20x_D6.S core_riscv.c system_ch32v20x.c debug.c main.c \
       usb_endp.c UART.c syscalls.c \
       $(HAL_SRCS) $(USB_CFG_SRCS) $(USB_DRV_SRCS)

OBJS = $(patsubst %.S,%.o,$(patsubst %.c,%.o,$(notdir $(SRCS))))

all: $(OUT_ELF) $(OUT_BIN)

$(OUT_ELF): $(OBJS)
	$(GCC) $(LDFLAGS) -o $@ $^ -lgcc -lc

%.o: %.c
	$(GCC) $(CFLAGS) -c -o $@ $<

%.o: %.S
	$(GCC) $(CFLAGS) -c -o $@ $<

$(OUT_BIN): $(OUT_ELF)
	$(OBJCOPY) -O binary $< $@

clean:
	rm -f $(OBJS) $(TARGET).elf $(TARGET).bin $(TARGET).hex $(TARGET).map
.PHONY: all clean
