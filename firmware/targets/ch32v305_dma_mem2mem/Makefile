TARGET   = ch32v305_dma_mem2mem
TOOLCHAIN = /nvme1t/wch-riscv-gcc/bin
GCC      = $(TOOLCHAIN)/riscv-none-embed-gcc
OBJCOPY  = $(TOOLCHAIN)/riscv-none-embed-objcopy
OUT_ELF ?= $(TARGET).elf
OUT_BIN ?= $(TARGET).bin
ARCH_FLAGS = -march=rv32imafcxw -mabi=ilp32f

HAL_INC = /nvme1t/github/ch32v307/EVT/EXAM/SRC/Peripheral/inc
HAL_SRC = /nvme1t/github/ch32v307/EVT/EXAM/SRC/Peripheral/src
VPATH = $(HAL_SRC)

CFLAGS = $(ARCH_FLAGS) -O2 -nostdlib -ffunction-sections -fdata-sections -I. -I$(HAL_INC) -Wall -DCH32V30x_D8C
LDFLAGS = $(ARCH_FLAGS) -T Link.ld -nostdlib -Wl,--gc-sections -Wl,-Map=$(TARGET).map
SRCS = startup_ch32v30x_D8C.S core_riscv.c system_ch32v30x.c debug.c main.c ch32v30x_it.c ch32v30x_rcc.c ch32v30x_dma.c
OBJS = $(patsubst %.S,%.o,$(patsubst %.c,%.o,$(notdir $(SRCS))))
all: $(OUT_ELF) $(OUT_BIN)
$(OUT_ELF): $(OBJS)
	$(GCC) $(LDFLAGS) -o $@ $^
%.o: %.c
	$(GCC) $(CFLAGS) -c -o $@ $<
%.o: %.S
	$(GCC) $(CFLAGS) -c -o $@ $<
$(OUT_BIN): $(OUT_ELF)
	$(OBJCOPY) -O binary $< $@
clean:
	rm -f $(OBJS) $(TARGET).elf $(TARGET).bin $(TARGET).hex $(TARGET).map
.PHONY: all clean
