# Build configuration for the ATmega328P (Arduino Nano) bare-metal firmware.
# Builds and flashes with only the AVR toolchain on PATH; no IDE or editor
# extension is involved.
# Works on Windows, macOS and Linux (see "Platform differences" below).
#
# Usage:
#   make              Build build/output.hex and print the size report
#   make size         Print the size report (builds first if needed)
#   make flash        Build if needed, then upload to the board over serial
#   make test         Build the core for this PC and run its unit tests
#   make misra        Run cppcheck with the MISRA addon over the source
#   make clean        Delete the build/ directory
#   make flash PORT=COM5   Override any variable below from the command line
#
# What a build does, in order:
#   1. Compile   each .c file     ->  build/obj/**/*.o   (one avr-gcc call per file)
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
    # For `make test`: the PC's own C compiler, the suffix it gives programs,
    # and how to run a program given its path.
    HOST_CC ?= gcc
    EXE     = .exe
    run     = $(subst /,\,$1)
else
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

# cppcheck is only needed for `make misra`, not for building or flashing. Its
# MISRA addon is a Python script, so python must be on PATH as well.
CPPCHECK = cppcheck

# ---- Files ------------------------------------------------------------------

BUILD_DIR = build
TARGET    = $(BUILD_DIR)/output

# The source is split by role (CLAUDE.md has the rules for what goes where):
#   core/             Pure sequencer logic; no hardware access, also builds on a PC
#   ports/            Headers describing what the core needs from the outside world
#   adapters/target/  Implementations of the ports for this MCU
#   app/              main.c, which wires the adapters to the core
# SRC_DIRS are the folders compiled into the firmware; INCLUDE_DIRS are searched
# for headers.
SRC_DIRS     = app core adapters/target
INCLUDE_DIRS = core ports adapters/target

# SRCS: every .c file in SRC_DIRS. A new .c file in one of those folders is
# picked up automatically; a new folder must be added to SRC_DIRS.
SRCS := $(foreach dir,$(SRC_DIRS),$(wildcard $(dir)/*.c))

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
.PHONY: all size flash clean misra test

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
$(BUILD_DIR)/obj/%.o: %.c
	@$(call mkdir_p,$(@D))
	$(CC) $(CFLAGS) -c -o $@ $<

# Step 4: size report. Reading the columns:
#   text + data = flash used   (ATmega328P has 32768 bytes; 30720 with the old
#                                bootloader, 32256 with the newer one)
#   data + bss  = static RAM   (2048 bytes total, shared with the stack)
size: $(TARGET).elf
	$(SIZE) $<

# ---- Host tests -------------------------------------------------------------
#
# `make test` compiles the hardware-free code with the PC's compiler (HOST_CC,
# not avr-gcc) and runs it. Nothing here touches the board or the firmware
# build; the programs land in build/host/. A test program exits non-zero if any
# check fails, which makes `make test` fail.
#   -std=c99 -pedantic    The core must be plain C99 with no compiler extensions
#   -Werror               Any warning fails the build
HOST_DIR    = $(BUILD_DIR)/host
HOST_CFLAGS = -std=c99 -pedantic -g -Wall -Wextra -Wconversion -Wshadow -Werror \
              -Icore -Iports -Iapp -Iadapters/host -Itests

CORE_SRCS := $(wildcard core/*.c)
HOST_SRCS := $(wildcard adapters/host/*.c)
# Any header change rebuilds the tests; simpler than tracking each one
HOST_HDRS := $(wildcard core/*.h ports/*.h app/*.h adapters/host/*.h tests/*.h)

TEST_SEQ = $(HOST_DIR)/test_seq$(EXE)
TEST_APP = $(HOST_DIR)/test_app$(EXE)

TEST_LOGGER = $(HOST_DIR)/test_serial_logger$(EXE)

test: $(TEST_SEQ) $(TEST_APP) $(TEST_LOGGER)
	$(call run,$(TEST_SEQ))
	$(call run,$(TEST_APP))
	$(call run,$(TEST_LOGGER))

# Tests for the target serial logger. The test includes the adapter's .c file
# directly, with the MCU register header swapped for tests/fake_atmega328p_regs.h,
# so it can check the exact bytes the logger would send. It needs the adapter's
# folder on the include path and the same F_CPU the firmware is built with.
$(TEST_LOGGER): tests/test_serial_logger.c adapters/target/serial_logger.c $(HOST_HDRS) $(wildcard adapters/target/*.h)
	@$(call mkdir_p,$(HOST_DIR))
	$(HOST_CC) $(HOST_CFLAGS) -Iadapters/target -DF_CPU=$(F_CPU) -o $@ tests/test_serial_logger.c

# Unit tests for the core alone: no ports, no adapters.
$(TEST_SEQ): tests/test_seq.c $(CORE_SRCS) $(HOST_HDRS)
	@$(call mkdir_p,$(HOST_DIR))
	$(HOST_CC) $(HOST_CFLAGS) -o $@ tests/test_seq.c $(CORE_SRCS)

# Tests for the application loop: the real app.c and core, linked against the
# fake ports in adapters/host instead of the MCU ones. main.c is left out; the
# test program supplies main.
$(TEST_APP): tests/test_app.c app/app.c $(CORE_SRCS) $(HOST_SRCS) $(HOST_HDRS)
	@$(call mkdir_p,$(HOST_DIR))
	$(HOST_CC) $(HOST_CFLAGS) -o $@ tests/test_app.c app/app.c $(CORE_SRCS) $(HOST_SRCS)

# Static analysis: cppcheck plus its MISRA addon (see CLAUDE.md for how findings
# are handled). tools/misra/misra.json points the addon at the rule headlines
# file, which is not in the repo; if it is missing, run
# tools/misra/fetch_misra_headlines.sh first. Exits non-zero if anything is found.
#
# The analysis runs twice, once per set of adapters, because cppcheck treats
# everything it is given as one program. The target and host adapters each
# define the same port functions (only one set is ever linked), and analysing
# both together would report every port as defined twice.
#   make misra         Both runs; stops at the first one that reports findings
#   make -k misra      Both runs even if the first reports findings
#   MISRA_COMMON    Folders in both runs
#   MISRA_INCLUDES  Where cppcheck looks for the project's own headers
#   -DF_CPU=...     Same define the compiler gets; without it the headers hit
#                     their #error and cppcheck skips the code
MISRA_COMMON   = core ports app
MISRA_INCLUDES = -I core -I ports
MISRA_FLAGS    = --addon=tools/misra/misra.json --std=c99 \
                 --enable=warning,style,performance,portability \
                 --inline-suppr --error-exitcode=1 -DF_CPU=$(F_CPU)

.PHONY: misra-target misra-host
misra: misra-target misra-host

misra-target:
	$(CPPCHECK) $(MISRA_FLAGS) $(MISRA_INCLUDES) $(MISRA_COMMON) adapters/target

misra-host:
	$(CPPCHECK) $(MISRA_FLAGS) $(MISRA_INCLUDES) $(MISRA_COMMON) adapters/host

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

# Pull in the compiler-generated .d files. Each one is a small makefile fragment
# like "build/obj/main.o: src/main.c src/app/init.h ...", which is what makes a
# header edit trigger a rebuild. The leading - means "don't complain if they
# don't exist yet", which is the case on a first or clean build.
-include $(DEPS)
