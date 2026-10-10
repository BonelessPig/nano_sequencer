# Build configuration for the ATmega328P (Arduino Nano) bare-metal firmware.
# Builds and flashes with only the AVR toolchain on PATH; no IDE or editor
# extension is involved.
# Works on Windows, macOS and Linux (see "Platform differences" below).
#
# Usage:
#   make              Build build/debug/output.hex and print the size report
#   make CONFIG=release   The same for the release build (see "Build configuration")
#   make size         Print the size report (builds first if needed)
#   make flash        Build if needed, then upload to the board over serial
#   make test         Build the code for this PC and run its unit tests
#   make coverage     Run the tests instrumented and report line/branch coverage
#   make misra        Run cppcheck with the MISRA addon over the source
#   make sim          Run both firmware images on an emulated board
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
# out the UART baud divisor, so it must match the crystal on the board.
MCU        = atmega328p
F_CPU      = 16000000UL

# ---- Build configuration ----------------------------------------------------

# CONFIG picks which logger is linked into the firmware. Nothing else differs:
# every file is compiled the same way for both, so they share build/obj/.
#   debug     serial_logger.c: text log over USART0 at 115200 baud (the default)
#   release   null_logger.c: no logging, and the USART is never switched on
# Each configuration gets its own output folder, so switching between them
# can never leave a stale image behind. CONFIG applies to `make`, `make size`
# and `make flash`; the tests and the analysis always cover both loggers.
CONFIG ?= debug

LOGGERS = serial_logger null_logger
ifeq ($(CONFIG),debug)
    LOGGER = serial_logger
else ifeq ($(CONFIG),release)
    LOGGER = null_logger
else
    $(error CONFIG must be debug or release, not "$(CONFIG)")
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

# cppcheck is only needed for `make misra`, not for building or flashing. Its
# MISRA addon is a Python script, so python must be on PATH as well.
CPPCHECK = cppcheck

# `make coverage` needs gcov, which comes with the PC's gcc, and python to
# turn its output into a table. (PYTHON is set per platform, above.)
GCOV = gcov

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

# SRCS: every .c file in SRC_DIRS, less the loggers this configuration does not
# use. A new .c file in one of those folders is picked up automatically; a new
# folder must be added to SRC_DIRS.
UNUSED_LOGGER_SRCS := $(patsubst %,adapters/target/%.c,$(filter-out $(LOGGER),$(LOGGERS)))
SRCS := $(filter-out $(UNUSED_LOGGER_SRCS),$(foreach dir,$(SRC_DIRS),$(wildcard $(dir)/*.c)))

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
.PHONY: all size flash clean misra test coverage

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

# `make coverage` runs this same section again with COVERAGE=1, which builds
# the programs with gcc's coverage instrumentation into their own folder.
#   --coverage   Count how often each line and branch runs
#   -O0          No optimization, so the counts match the source line for line
ifdef COVERAGE
    HOST_DIR     = $(BUILD_DIR)/coverage
    HOST_CFLAGS += --coverage -O0
endif

TEST_SEQ = $(HOST_DIR)/test_seq$(EXE)
TEST_APP = $(HOST_DIR)/test_app$(EXE)

# Tests that #include the source file they test (one each for the target
# adapters and main.c) rather than linking it. tests/test_<name>.c builds into
# the program test_<name>; add new ones to this list.
INCLUDING_TESTS = test_serial_logger test_null_logger test_register_init \
                  test_analog_reader test_shift_reg_reader test_init test_delay \
                  test_main
TEST_INCLUDING := $(patsubst %,$(HOST_DIR)/%$(EXE),$(INCLUDING_TESTS))

TEST_PROGRAMS = $(TEST_SEQ) $(TEST_APP) $(TEST_INCLUDING)

# One recipe line that runs one test program; the blank line is what separates
# the lines when $(foreach) below strings several of these together.
define run_test
	$(call run,$1)

endef

test: $(TEST_PROGRAMS)
	$(foreach program,$(TEST_PROGRAMS),$(call run_test,$(program)))

# Tests for the target adapters and main.c. Each test includes the .c file it
# tests directly, with the MCU register header swapped for
# tests/fake_atmega328p_regs.h, so it can drive the fake registers and see what
# the code does with them. Functions the tested file calls but does not define
# are stubs in the test. These need the adapters' folder on the include path
# and the same F_CPU the firmware is built with.
$(TEST_INCLUDING): $(HOST_DIR)/%$(EXE): tests/%.c app/main.c $(HOST_HDRS) $(wildcard adapters/target/*.c adapters/target/*.h)
	@$(call mkdir_p,$(HOST_DIR))
	$(HOST_CC) $(HOST_CFLAGS) -Iadapters/target -DF_CPU=$(F_CPU) -o $@ $<

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

# Coverage: rebuild the tests instrumented, run them, and report which lines
# and branches of the source they exercised. Starts from an empty folder each
# time so counts from an earlier run cannot leak in. The report fails unless
# every firmware source file is at 100% of lines and branches; see the top of
# tools/coverage/report.py for exactly what is counted.
coverage:
	@$(call rm_rf,$(BUILD_DIR)/coverage)
	$(MAKE) test COVERAGE=1
	$(PYTHON) tools/coverage/report.py $(GCOV) $(BUILD_DIR)/coverage

# Static analysis: cppcheck plus its MISRA addon (see tools/misra/CLAUDE.md for how
# findings are handled). tools/misra/misra.json points the addon at the rule headlines
# file, which is not in the repo; if it is missing, run
# tools/misra/fetch_misra_headlines.sh first. Exits non-zero if anything is found.
#
# The analysis runs three times, once per program that is actually linked,
# because cppcheck treats everything it is given as one program. The target and
# host adapters each define the same port functions, as do the two target
# loggers (only one of each is ever linked), and analysing them together would
# report every such function as defined twice.
#
# Each run analyses the same files that are linked together for that build:
#   misra-debug    core, ports, all of app (including main.c), adapters/target
#                    without null_logger.c
#   misra-release  the same without serial_logger.c
#   misra-host     core, ports, app/app.c, adapters/host, tests/test_app.c
# The host run mirrors the test_app program. It leaves out app/main.c because
# the test supplies its own main, and it includes the test file only so
# cppcheck can see that the host fakes' control functions are called from
# another file. Test code is not held to the coding standard, so findings
# located in tests/ are not reported (MISRA_HOST_SCOPE).
#   make misra         All runs; stops at the first one that reports findings
#   make -k misra      All runs even if an earlier one reports findings
#   -i <file>       Leave that file out of the run
#   MISRA_INCLUDES  Where cppcheck looks for the project's own headers
#   -DF_CPU=...     Same define the compiler gets; without it the headers hit
#                     their #error and cppcheck skips the code
MISRA_INCLUDES   = -I core -I ports
MISRA_FLAGS      = --addon=tools/misra/misra.json --std=c99 \
                   --enable=warning,style,performance,portability \
                   --inline-suppr --error-exitcode=1 -DF_CPU=$(F_CPU)
MISRA_HOST_SCOPE = -I app -I adapters/host -I tests "--suppress=*:tests/*"

.PHONY: misra-debug misra-release misra-host
misra: misra-debug misra-release misra-host

misra-debug:
	$(CPPCHECK) $(MISRA_FLAGS) $(MISRA_INCLUDES) -i adapters/target/null_logger.c core ports app adapters/target

misra-release:
	$(CPPCHECK) $(MISRA_FLAGS) $(MISRA_INCLUDES) -i adapters/target/serial_logger.c core ports app adapters/target

misra-host:
	$(CPPCHECK) $(MISRA_FLAGS) $(MISRA_INCLUDES) $(MISRA_HOST_SCOPE) core ports app/app.c adapters/host tests/test_app.c

# ---- Emulation --------------------------------------------------------------
#
# `make sim` runs both firmware images, exactly as they would be flashed, on an
# emulated ATmega328P (the avr8js library, under Node) with the board's
# external parts modelled in tools/sim/: the 74HC165 chain, the tempo pot and
# a serial capture. The scenarios in tools/sim/scenarios/ assert on what the
# firmware logs, the pulses it sends and how long a step takes, counted in CPU
# cycles. Nothing here touches the board.
#
# The first run downloads avr8js into tools/sim/node_modules (needs a network
# connection once); after that it only reinstalls if package-lock.json changes.
NODE ?= node
NPM  ?= npm
SIM_DIR     = tools/sim
SIM_MODULES = $(SIM_DIR)/node_modules/.package-lock.json

.PHONY: sim
sim: $(SIM_MODULES)
	$(MAKE) CONFIG=debug $(BUILD_DIR)/debug/output.hex
	$(MAKE) CONFIG=release $(BUILD_DIR)/release/output.hex
	$(NODE) --test --test-reporter=spec "$(SIM_DIR)/scenarios/*.test.js"

$(SIM_MODULES): $(SIM_DIR)/package-lock.json
	$(NPM) --prefix $(SIM_DIR) ci

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
