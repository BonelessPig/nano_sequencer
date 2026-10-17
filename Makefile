# Build configuration for the ATmega328P (Arduino Nano) bare-metal firmware.
# Builds and flashes with only the AVR toolchain on PATH; no IDE or editor
# extension is involved.
# Works on Windows, macOS and Linux (see "Platform differences" below).
#
# Usage:
#   make              Build build/debug/output.hex and print the size report
#   make CONFIG=release   The same for the release build (see "Build configuration")
#   make CONFIG=midi      The same for the build that sends MIDI
#   make size         Print the size report (builds first if needed)
#   make flash        Build if needed, then upload to the board over serial
#   make test         Build the code for this PC and run its unit tests
#   make test-quiet   The same, printing one line unless a test fails
#   make coverage     Run the tests instrumented and report line/branch coverage
#   make misra        Run cppcheck with the MISRA addon over the source
#   make sim          Run the firmware images on an emulated board
#   make live         Run one image on that board at real speed, shown in a browser
#   make check        Run all of the above checks; one line each unless one fails
#   make figures      Measure the sizes and counts the documents quote
#   make file-sizes   List the files grown past the size the rules allow
#   make clean       Delete the build/ directory
#   make flash PORT=COM5   Override any variable below from the command line
#
# What a build does, in order:
#   1. Compile   each .c file     ->  build/obj/**/*.o   (one avr-gcc call per file)
#   2. Link      all .o files     ->  build/<config>/output.elf   (adds startup code + libgcc)
#   3. Convert   output.elf       ->  build/<config>/output.hex   (the format avrdude uploads)
#   4. Report    flash and RAM usage of output.elf
#
# make only redoes a step when its inputs are newer than its output, so a second
# `make` with no edits does nothing except print the size report.
#
# Debugging tips:
#   make -n           Print the commands that would run, without running them
#   make -B           Rebuild everything regardless of timestamps
#   make clean        If the build ever behaves oddly, start from scratch
#
# The host tests and coverage are in tests/tests.mk, and the analysis, the
# emulated board, `make check` and `make figures` in tools/checks.mk. Both
# are included at the bottom of this file and use what it defines.

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
    # For `make test`: the PC's own C compiler, the suffix it gives programs,
    # and how to run a program given its path.
    HOST_CC ?= gcc
    EXE     = .exe
    run     = $(subst /,\,$1)
    PYTHON ?= python
else
    PYTHON ?= python3
    mkdir_p = mkdir -p "$1"
    rm_rf   = rm -rf "$1"
    HOST_CC ?= cc
    EXE     =
    run     = ./$1
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
# out the UART baud divisor and the timer period of the 1 kHz tick, so it must
# match the crystal on the board.
MCU        = atmega328p
F_CPU      = 16000000UL

# ---- Build configuration ----------------------------------------------------

# CONFIG picks which logger and which MIDI output are linked into the firmware,
# one of each. Nothing else differs: every file is compiled the same way for
# all of them, so they share build/obj/. There is one USART, so at most one of
# the two uses it.
#   debug     serial_logger.c: text log over USART0 at 115200 baud (the default)
#             null_midi_output.c, null_midi_input.c: no MIDI
#   release   null_logger.c: no logging
#             null_midi_output.c, null_midi_input.c: no MIDI, and the USART
#             is never switched on
#   midi      null_logger.c: no logging
#             midi_output.c: MIDI out over USART0 at 31250 baud
#             midi_input.c: MIDI in over the same USART
# Each configuration gets its own output folder, so switching between them
# can never leave a stale image behind. CONFIG applies to `make`, `make size`
# and `make flash`; the tests and the analysis always cover every one of
# these files.
CONFIG ?= debug

LOGGERS      = serial_logger null_logger
MIDI_OUTPUTS = midi_output null_midi_output
MIDI_INPUTS  = midi_input null_midi_input
ifeq ($(CONFIG),debug)
    LOGGER      = serial_logger
    MIDI_OUTPUT = null_midi_output
    MIDI_INPUT  = null_midi_input
else ifeq ($(CONFIG),release)
    LOGGER      = null_logger
    MIDI_OUTPUT = null_midi_output
    MIDI_INPUT  = null_midi_input
else ifeq ($(CONFIG),midi)
    LOGGER      = null_logger
    MIDI_OUTPUT = midi_output
    MIDI_INPUT  = midi_input
else
    $(error CONFIG must be debug, release or midi, not "$(CONFIG)")
endif

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

BUILD_DIR = build
TARGET    = $(BUILD_DIR)/$(CONFIG)/output

# The source is split by role (CLAUDE.md has the rules for what goes where):
#   core/             Pure sequencer logic; no hardware access, also builds on a PC
#   ports/            Headers describing what the core needs from the outside world
#   adapters/target/  Implementations of the ports for this MCU
#   app/              main.c, which wires the adapters to the core
# SRC_DIRS are the folders compiled into the firmware; INCLUDE_DIRS are searched
# for headers.
SRC_DIRS     = app core adapters/target
INCLUDE_DIRS = core ports adapters/target

# SRCS: every .c file in SRC_DIRS, less the logger, the MIDI output and the
# MIDI input this configuration does not use. A new .c file in one of those folders is picked
# up automatically; a new folder must be added to SRC_DIRS.
USED_CHOICES       := $(LOGGER) $(MIDI_OUTPUT) $(MIDI_INPUT)
UNUSED_CHOICES     := $(filter-out $(USED_CHOICES),$(LOGGERS) $(MIDI_OUTPUTS) $(MIDI_INPUTS))
UNUSED_CHOICE_SRCS := $(patsubst %,adapters/target/%.c,$(UNUSED_CHOICES))
SRCS := $(filter-out $(UNUSED_CHOICE_SRCS),$(foreach dir,$(SRC_DIRS),$(wildcard $(dir)/*.c)))

# OBJS: the matching object file for each source, mirrored under build/obj/
# (app/main.c -> build/obj/app/main.o).
OBJS := $(patsubst %.c,$(BUILD_DIR)/obj/%.o,$(SRCS))

# DEPS: one .d file per object, written by the compiler (see -MMD below). Each
# lists the headers that source file includes.
DEPS := $(OBJS:.o=.d)

# ---- Flags ------------------------------------------------------------------

# Compiler flags:
#   -g                    Include debug info in the .elf (not uploaded to the chip)
#   -Os                   Optimize for size
#   -std=c99              Compile as C99
#   -Wall -Wextra         Enable the common warnings
#   -Wconversion          Warn when an implicit conversion could change a value or its sign
#   -Wshadow              Warn when a local name hides another variable
#   -Werror               Treat every warning as an error, so none can be ignored
#   -ffunction-sections   Put each function and variable in its own section so
#   -fdata-sections         the linker can drop the ones nothing uses
#   -pipe                 Pass data between compiler stages in memory, not temp files
#   -ffreestanding        No standard library is assumed, so <stdint.h>, <stdbool.h>
#                           and <stddef.h> come from the compiler itself, not avr-libc
#   -mmcu=...             Target chip
#   -DF_CPU=...           Defines F_CPU for the C code
#   -I<dir>               One per INCLUDE_DIRS entry, so headers are found by name
#   -MMD -MP              Also write a .d file listing the headers each source
#                           includes, so editing a header rebuilds what uses it
CFLAGS  = -g -Os -std=c99 -Wall -Wextra -Wconversion -Wshadow -Werror \
          -ffunction-sections -fdata-sections -pipe -ffreestanding \
          -mmcu=$(MCU) -DF_CPU=$(F_CPU) $(addprefix -I,$(INCLUDE_DIRS)) \
          -MMD -MP

# Linker flags:
#   -mmcu=...             Picks the startup code and memory map for the chip
#   -Wl,--gc-sections     Remove unused functions/variables (pairs with the
#                           -ffunction-sections / -fdata-sections flags above)
#   -Wl,-Map=...          Write output.map beside the .elf: what the linker put
#                           where, and from which object. `make figures` reads it
LDFLAGS = -mmcu=$(MCU) -Wl,--gc-sections -Wl,-Map=$(TARGET).map

# Register addresses. The C code declares each MCU register as a plain variable
# (adapters/target/atmega328p_regs.h) and this linker file says where each one
# is. It is given to the linker as an extra input, which adds its symbols to
# the toolchain's standard memory layout rather than replacing it. An
# "undefined reference to `PORTD'" style error means a register is declared in
# the header but has no address in this file.
REGS_LD = adapters/target/atmega328p_regs.ld

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
.PHONY: all size flash clean images

# Default target (what plain `make` runs): build the .hex, then report size.
all: $(TARGET).hex size

# Step 3: convert the .elf to Intel HEX. -R .eeprom leaves out EEPROM data,
# which is not part of the flash image.
$(TARGET).hex: $(TARGET).elf
	$(OBJCOPY) -O ihex -R .eeprom $< $@

# Step 2: link all object files into one .elf. An "undefined reference" error
# here means a function is declared and called but its .c file is not in SRCS.
$(TARGET).elf: $(OBJS) $(REGS_LD)
	@$(call mkdir_p,$(@D))
	$(CC) $(LDFLAGS) -o $@ $(OBJS) $(REGS_LD)

# Step 1: compile one .c file to one .o file. The first line creates the output
# folder if it is missing. Compile errors and warnings are reported here, per file.
$(BUILD_DIR)/obj/%.o: %.c
	@$(call mkdir_p,$(@D))
	$(CC) $(CFLAGS) -c -o $@ $<

# Step 4: size report. Reading the columns:
#   text + data = flash used   (ATmega328P has 32768 bytes; 30720 with the old
#                                bootloader, 32256 with the newer one)
#   data + bss  = static RAM   (2048 bytes total, shared with the stack)
size: $(TARGET).elf
	$(SIZE) $<

# All three images, for the targets that need every one of them whatever
# CONFIG is.
images:
	$(MAKE) CONFIG=debug $(BUILD_DIR)/debug/output.hex
	$(MAKE) CONFIG=release $(BUILD_DIR)/release/output.hex
	$(MAKE) CONFIG=midi $(BUILD_DIR)/midi/output.hex

# Upload the .hex to the board through the bootloader. The first line stops with
# a clear message if no port was given and none could be auto-detected.
# After writing, avrdude reads the flash back and compares it to the .hex, so a
# "verification error" means the upload did not take; try again. avrdude also
# checks the chip's signature first, so a "signature mismatch" error means the
# board is not an ATmega328P.
#   -U flash:w:<file>:i  write <file> to flash, i = Intel HEX format
flash: $(TARGET).hex
	$(if $(PORT),,$(error No serial port found. Plug in the board or pass PORT=<device>))
	$(AVRDUDE) -c $(PROGRAMMER) -p ATMEGA328P -P $(PORT) -b $(BAUD) -U flash:w:$<:i

clean:
	@$(call rm_rf,$(BUILD_DIR))

# ---- Tests and checks -----------------------------------------------------

include tests/tests.mk
include tools/checks.mk

# Pull in the compiler-generated .d files. Each one is a small makefile fragment
# like "build/obj/main.o: src/main.c src/app/init.h ...", which is what makes a
# header edit trigger a rebuild. The leading - means "don't complain if they
# don't exist yet", which is the case on a first or clean build.
-include $(DEPS)
