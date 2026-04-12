TARGET  = ch32v203_blinky
GCC     = /nvme1t/wch-riscv-gcc/bin/riscv-none-embed-gcc
OBJCOPY = /nvme1t/wch-riscv-gcc/bin/riscv-none-embed-objcopy

CFLAGS  = -march=rv32imac -mabi=ilp32 -Os -Wall \
          -ffunction-sections -fdata-sections \
          -nostdlib \
          -I.

LDFLAGS = -T Link.ld -Wl,--gc-sections -Wl,-Map=$(TARGET).map

SRCS    = startup_ch32v20x_D6.S \
          core_riscv.c \
          system_ch32v20x.c \
          debug.c \
          main.c

OBJS    = $(SRCS:.c=.o)
OBJS   := $(OBJS:.S=.o)

all: $(TARGET).elf $(TARGET).bin $(TARGET).hex

$(TARGET).elf: $(OBJS)
	$(GCC) $(CFLAGS) $(LDFLAGS) -o $@ $^

%.o: %.c
	$(GCC) $(CFLAGS) -c -o $@ $<

%.o: %.S
	$(GCC) $(CFLAGS) -c -o $@ $<

$(TARGET).bin: $(TARGET).elf
	$(OBJCOPY) -O binary $< $@

$(TARGET).hex: $(TARGET).elf
	$(OBJCOPY) -O ihex $< $@

clean:
	rm -f *.o *.elf *.bin *.hex *.map
