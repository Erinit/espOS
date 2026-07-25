# Custom Bare-Metal ESP32 Makefile

# Toolchain definitions
CC      = xtensa-esp32-elf-gcc
LD      = xtensa-esp32-elf-ld
ESPTOOL = esptool.py

# Serial Port Configuration (Adjust PORT to match your system)
PORT    ?= /dev/ttyUSB0
BAUD    ?= 115200

# Compiler and Linker Flags
# -ffreestanding, -nostdlib, -fno-builtin: Enforce bare-metal compilation with zero stdlib bloat
# -mlongcalls: Required for Xtensa target memory offsets
# -g: Include debug symbols (stripped from final .bin, but useful in .elf)
CFLAGS  = -O2 -g -ffreestanding -nostdlib -fno-builtin -Wall -Wextra -mlongcalls -mtext-section-literals
LDFLAGS = -T kernel.ld -nostdlib -Map=os.map

# Automatic Source Detection (finds all .c and .S files in the directory)
C_SRCS  = $(wildcard *.c)
S_SRCS  = $(wildcard *.S)
OBJS    = $(C_SRCS:.c=.o) $(S_SRCS:.S=.o)

.PHONY: all clean flash monitor

all: os.bin

# Generate ESP32 bootable image from ELF
# A raw objcopy binary will fail to boot; the ROM requires the esptool image format
os.bin: os.elf
	$(ESPTOOL) --chip esp32 elf2image --flash_mode dio --flash_freq 40m --flash_size 4MB -o $@ $<

# Link object files into ELF container
os.elf: $(OBJS)
	$(LD) $(LDFLAGS) -o $@ $^

# Compile C sources
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Assemble assembly sources
%.o: %.S
	$(CC) $(CFLAGS) -c $< -o $@

# Flash the binary directly into the ESP32
flash: os.bin
	ls /dev/ttyUSB* 
	sudo chmod 666 $(PORT)
	$(ESPTOOL) --chip esp32 --port $(PORT) --baud $(BAUD) write-flash 0x1000 os.bin


# Simple serial monitor (Requires python-serial/miniterm, or adjust to use screen/picocom)
monitor:
	python3 -m serial.tools.miniterm $(PORT) $(BAUD)

# Clean build artifacts
clean:
	rm -f *.o *.elf *.bin *.map

show:
	screen /dev/ttyUSB0 115200