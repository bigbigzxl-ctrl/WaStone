TARGET   = ch32v003_minimal_mailbox
TOOLCHAIN = /nvme1t/wch-riscv-gcc/bin
GCC      = $(TOOLCHAIN)/riscv-none-embed-gcc
OBJCOPY  = $(TOOLCHAIN)/riscv-none-embed-objcopy

# AEL build_stm32 passes these; fall back to local names if not set
OUT_ELF ?= $(TARGET).elf
OUT_BIN ?= $(TARGET).bin

# CH32V003: RV32EC, ILP32E ABI, no stdlib
ARCH_FLAGS = -march=rv32ec -mabi=ilp32e
CFLAGS = $(ARCH_FLAGS) \
         -O2 -g \
         -nostdlib \
         -ffunction-sections -fdata-sections \
         -I. -I.. \
         -Wall

LDFLAGS = $(ARCH_FLAGS) \
          -T ch32v003.ld \
          -nostdlib \
          -Wl,--gc-sections \
          -Wl,-Map=$(TARGET).map

SRCS = main.c ch32v003fun.c
OBJS = $(SRCS:.c=.o)

all: $(OUT_ELF) $(OUT_BIN)

$(OUT_ELF): $(OBJS)
	$(GCC) $(LDFLAGS) -o $@ $^

%.o: %.c
	$(GCC) $(CFLAGS) -c -o $@ $<

$(OUT_BIN): $(OUT_ELF)
	$(OBJCOPY) -O binary $< $@

$(TARGET).hex: $(OUT_ELF)
	$(OBJCOPY) -O ihex $< $@

clean:
	rm -f $(OBJS) $(TARGET).elf $(TARGET).bin $(TARGET).hex $(TARGET).map $(OUT_ELF) $(OUT_BIN)

.PHONY: all clean
