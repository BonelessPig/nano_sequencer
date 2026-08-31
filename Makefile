# Build configuration for the ATmega328P (Arduino Nano) bare-metal firmware.
# Mirrors the flags used by the VS Code AVR extension (see .vscode/settings.json)
# so `make` produces the same binary without requiring the extension.

ifeq ($(OS),Windows_NT)
SHELL := cmd.exe
endif

MCU        = atmega328p
F_CPU      = 16000000UL
PROGRAMMER = arduino
PORT      ?= COM3
BAUD       = 57600

CC      = avr-gcc
OBJCOPY = avr-objcopy
AVRDUDE = avrdude

SRC_DIR   = src
BUILD_DIR = build
TARGET    = $(BUILD_DIR)/output

SRCS := $(wildcard $(SRC_DIR)/*.c) $(wildcard $(SRC_DIR)/app/*.c) $(wildcard $(SRC_DIR)/common/*.c)
OBJS := $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/obj/%.o,$(SRCS))

CFLAGS  = -g -Os -Wall -Wextra -fno-exceptions \
          -ffunction-sections -fdata-sections -pipe \
          -mmcu=$(MCU) -DF_CPU=$(F_CPU) -I$(SRC_DIR)
LDFLAGS = -mmcu=$(MCU) -Wl,--gc-sections

.PHONY: all flash clean

all: $(TARGET).hex

$(TARGET).hex: $(TARGET).elf
	$(OBJCOPY) -O ihex -R .eeprom $< $@

$(TARGET).elf: $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $(OBJS)

$(BUILD_DIR)/obj/%.o: $(SRC_DIR)/%.c
	@if not exist "$(subst /,\,$(@D))" mkdir "$(subst /,\,$(@D))"
	$(CC) $(CFLAGS) -c -o $@ $<

flash: $(TARGET).hex
	$(AVRDUDE) -F -V -c $(PROGRAMMER) -p ATMEGA328P -P $(PORT) -b $(BAUD) -U flash:w:$<:i

clean:
	@if exist "$(BUILD_DIR)" rmdir /s /q "$(BUILD_DIR)"
