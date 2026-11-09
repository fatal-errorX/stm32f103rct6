TARGET  = bootloader
PREFIX ?= arm-none-eabi-

CC      = $(PREFIX)gcc
OBJCOPY = $(PREFIX)objcopy
SIZE    = $(PREFIX)size

SRCS    = startup.c main.c hal.c crc.c
OBJS    = $(SRCS:%.c=build/%.o)

CFLAGS  = -mcpu=cortex-m3 -mthumb -Os -g3 -std=c11 -Wall -Wextra \
          -ffreestanding -ffunction-sections -fdata-sections
LDFLAGS = -mcpu=cortex-m3 -mthumb -nostartfiles --specs=nano.specs --specs=nosys.specs \
          -Tstm32f103rc_bl.ld -Wl,--gc-sections -Wl,-Map=build/$(TARGET).map

all: build/$(TARGET).elf build/$(TARGET).bin build/$(TARGET).hex
	$(SIZE) build/$(TARGET).elf

build/%.o: %.c config.h regs.h hal.h crc.h | build
	$(CC) $(CFLAGS) -c $< -o $@

build/$(TARGET).elf: $(OBJS) stm32f103rc_bl.ld
	$(CC) $(LDFLAGS) $(OBJS) -o $@

build/%.bin: build/%.elf
	$(OBJCOPY) -O binary $< $@

build/%.hex: build/%.elf
	$(OBJCOPY) -O ihex $< $@

build:
	mkdir -p build

clean:
	rm -rf build

.PHONY: all clean
