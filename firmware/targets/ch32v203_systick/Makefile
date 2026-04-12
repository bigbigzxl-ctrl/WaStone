TARGET   = ch32v203_systick
TOOLCHAIN = /nvme1t/wch-riscv-gcc/bin
GCC      = $(TOOLCHAIN)/riscv-none-embed-gcc
OBJCOPY  = $(TOOLCHAIN)/riscv-none-embed-objcopy
OUT_ELF ?= $(TARGET).elf
OUT_BIN ?= $(TARGET).bin
ARCH_FLAGS = -march=rv32imac -mabi=ilp32
CFLAGS = $(ARCH_FLAGS) -O2 -g -nostdlib -ffunction-sections -fdata-sections -I. -Wall
LDFLAGS = $(ARCH_FLAGS) -T Link.ld -nostdlib -Wl,--gc-sections -Wl,-Map=$(TARGET).map
SRCS = startup_ch32v20x_D6.S core_riscv.c system_ch32v20x.c debug.c main.c
OBJS = $(patsubst %.S,%.o,$(patsubst %.c,%.o,$(SRCS)))
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
