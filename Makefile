# Build configuration for the ATmega328P (Arduino Nano) bare-metal firmware.
# Mirrors the flags used by the VS Code AVR extension (see .vscode/settings.json)
# so `make` produces the same binary without requiring the extension.
# Works on Windows, macOS and Linux (see "Platform differences" below).
#
# Usage:
#   make              Build build/output.hex and print the size report
#   make size         Print the size report (builds first if needed)
#   make flash        Build if needed, then upload to the board over serial
#   make clean        Delete the build/ directory
#   make flash PORT=COM5   Override any variable below from the command line
#
# What a build does, in order:
#   1. Compile   each src/**/*.c  ->  build/obj/**/*.o   (one avr-gcc call per file)
#   2. Link      all .o files     ->  build/output.elf   (adds startup code + libgcc)
#   3. Convert   output.elf       ->  build/output.hex   (the format avrdude uploads)
#   4. Report    flash and RAM usage of output.elf
#
# make only redoes a step when its inputs are newer than its output, so a second
# `make` with no edits does nothing except print the size report.
#
# Debugging tips:
#   make -n           Print the commands that would run, without running them
#   make -B           Rebuild everything regardless of timestamps
#   make clean        If the build ever behaves oddly, start from scratch

# ---- Platform differences ---------------------------------------------------
#
# Only three things differ between Windows, macOS and Linux: how to create a
# folder, how to delete one, and what the board's serial port is called. Each
# branch below defines those, and the rest of the file is shared.
#
#   mkdir_p / rm_rf   Used as $(call mkdir_p,some/dir) in the rules further down;
#                     $1 is replaced by the argument.
#   DEFAULT_PORT      Where `make flash` looks for the board unless PORT is given.
#
# Windows sets the OS environment variable to Windows_NT; elsewhere it is unset,
# so `uname -s` is used to tell macOS (Darwin) from Linux.
ifeq ($(OS),Windows_NT)
    # Force cmd.exe instead of whatever sh.exe make finds on PATH, since the two
    # commands below are cmd.exe syntax. $(subst /,\,...) converts to backslashes.
    SHELL := cmd.exe
    mkdir_p = if not exist "$(subst /,\,$1)" mkdir "$(subst /,\,$1)"
    rm_rf   = if exist "$(subst /,\,$1)" rmdir /s /q "$(subst /,\,$1)"
    DEFAULT_PORT = COM3
else
    mkdir_p = mkdir -p "$1"
    rm_rf   = rm -rf "$1"
    # The port name varies with the board's USB-serial chip and which USB socket
    # it is in, so take the first device that matches the usual patterns. If the
    # wrong one is picked (several serial devices attached), pass PORT= yourself.
    ifeq ($(shell uname -s),Darwin)
        DEFAULT_PORT = $(firstword $(wildcard /dev/cu.usbserial* /dev/cu.wchusbserial* /dev/cu.usbmodem*))
    else
        DEFAULT_PORT = $(firstword $(wildcard /dev/ttyUSB* /dev/ttyACM*))
    endif
endif

# ---- Target hardware --------------------------------------------------------

# MCU selects the instruction set, the startup object (crtatmega328p.o) and the
# linker memory layout. F_CPU is the clock speed in Hz; the code uses it to work
# out the UART baud divisor, so it must match the crystal on the board.
MCU        = atmega328p
F_CPU      = 16000000UL

# ---- Upload settings --------------------------------------------------------

# PROGRAMMER "arduino" talks to the serial bootloader already on the Nano.
# BAUD 57600 is the old-bootloader Nano speed; newer bootloaders use 115200.
# If flashing fails with "not in sync", the usual causes are the wrong PORT or
# the wrong BAUD. PORT uses ?= so it can be overridden: make flash PORT=COM5
# On Linux, "permission denied" on the port means your user is not in the group
# that owns it (usually dialout): sudo usermod -aG dialout $USER, then log in again.
PROGRAMMER = arduino
PORT      ?= $(DEFAULT_PORT)
BAUD       = 57600

# ---- Tools (must be on PATH) ------------------------------------------------

CC      = avr-gcc
OBJCOPY = avr-objcopy
SIZE    = avr-size
AVRDUDE = avrdude

# ---- Files ------------------------------------------------------------------

SRC_DIR   = src
BUILD_DIR = build
TARGET    = $(BUILD_DIR)/output

# SRCS: every .c file in src/, src/app/ and src/common/. A new .c file in one of
# those folders is picked up automatically; a new folder must be added here.
SRCS := $(wildcard $(SRC_DIR)/*.c) $(wildcard $(SRC_DIR)/app/*.c) $(wildcard $(SRC_DIR)/common/*.c)

# OBJS: the matching object file for each source, mirrored under build/obj/
# (src/app/init.c -> build/obj/app/init.o).
OBJS := $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/obj/%.o,$(SRCS))

# DEPS: one .d file per object, written by the compiler (see -MMD below). Each
# lists the headers that source file includes.
DEPS := $(OBJS:.o=.d)

# ---- Flags ------------------------------------------------------------------

# Compiler flags:
#   -g                    Include debug info in the .elf (not uploaded to the chip)
#   -Os                   Optimize for size
#   -Wall -Wextra         Enable the common warnings
#   -ffunction-sections   Put each function and variable in its own section so
#   -fdata-sections         the linker can drop the ones nothing uses
#   -pipe                 Pass data between compiler stages in memory, not temp files
#   -mmcu=...             Target chip
#   -DF_CPU=...           Defines F_CPU for the C code
#   -I src                Lets code include headers by path from src/
#   -MMD -MP              Also write a .d file listing the headers each source
#                           includes, so editing a header rebuilds what uses it
CFLAGS  = -g -Os -Wall -Wextra \
          -ffunction-sections -fdata-sections -pipe \
          -mmcu=$(MCU) -DF_CPU=$(F_CPU) -I$(SRC_DIR) \
          -MMD -MP

# Linker flags:
#   -mmcu=...             Picks the startup code and memory map for the chip
#   -Wl,--gc-sections     Remove unused functions/variables (pairs with the
#                           -ffunction-sections / -fdata-sections flags above)
LDFLAGS = -mmcu=$(MCU) -Wl,--gc-sections

# ---- Rules ------------------------------------------------------------------
#
# Rule syntax:   output: inputs
#                <TAB>command
# Recipe lines must start with a real tab character, not spaces. Inside a recipe:
#   $@  = the output file of the rule
#   $<  = the first input file
#   $(@D) = the directory part of the output file
# A leading @ on a command stops make from echoing it.

# These names are commands, not files, so always run them when asked.
.PHONY: all size flash clean

# Default target (what plain `make` runs): build the .hex, then report size.
all: $(TARGET).hex size

# Step 3: convert the .elf to Intel HEX. -R .eeprom leaves out EEPROM data,
# which is not part of the flash image.
$(TARGET).hex: $(TARGET).elf
	$(OBJCOPY) -O ihex -R .eeprom $< $@

# Step 2: link all object files into one .elf. An "undefined reference" error
# here means a function is declared and called but its .c file is not in SRCS.
$(TARGET).elf: $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $(OBJS)

# Step 1: compile one .c file to one .o file. The first line creates the output
# folder if it is missing. Compile errors and warnings are reported here, per file.
$(BUILD_DIR)/obj/%.o: $(SRC_DIR)/%.c
	@$(call mkdir_p,$(@D))
	$(CC) $(CFLAGS) -c -o $@ $<

# Step 4: size report. Reading the columns:
#   text + data = flash used   (ATmega328P has 32768 bytes; 30720 with the old
#                                bootloader, 32256 with the newer one)
#   data + bss  = static RAM   (2048 bytes total, shared with the stack)
size: $(TARGET).elf
	$(SIZE) $<

# Upload the .hex to the board through the bootloader. The first line stops with
# a clear message if no port was given and none could be auto-detected.
# After writing, avrdude reads the flash back and compares it to the .hex, so a
# "verification error" means the upload did not take; try again.
#   -F  skip the device signature check (some Nano clones report a different
#       chip signature; without -F avrdude would refuse to flash those)
#   -U flash:w:<file>:i  write <file> to flash, i = Intel HEX format
flash: $(TARGET).hex
	$(if $(PORT),,$(error No serial port found. Plug in the board or pass PORT=<device>))
	$(AVRDUDE) -F -c $(PROGRAMMER) -p ATMEGA328P -P $(PORT) -b $(BAUD) -U flash:w:$<:i

clean:
	@$(call rm_rf,$(BUILD_DIR))

# Pull in the compiler-generated .d files. Each one is a small makefile fragment
# like "build/obj/main.o: src/main.c src/app/init.h ...", which is what makes a
# header edit trigger a rebuild. The leading - means "don't complain if they
# don't exist yet", which is the case on a first or clean build.
-include $(DEPS)
