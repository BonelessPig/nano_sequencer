# The host tests and their coverage: `make test` and `make coverage`.
# Included by the Makefile, which defines the folders, the platform's
# commands and F_CPU.

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

# `make coverage` needs gcov, which comes with the PC's gcc, and python to
# turn its output into a table. (PYTHON is set per platform, in the Makefile.)
GCOV = gcov

# `make coverage` runs this same section again with COVERAGE=1, which builds
# the programs with gcc's coverage instrumentation into their own folder.
#   --coverage   Count how often each line and branch runs
#   -O0          No optimization, so the counts match the source line for line
ifdef COVERAGE
    HOST_DIR     = $(BUILD_DIR)/coverage
    HOST_CFLAGS += --coverage -O0
endif

# Every tests/test_<name>.c builds into the program test_<name>, and is one
# of three kinds by how it is linked. A new one is found by its name: it is
# a test of the core unless it is named test_app_<topic> or is added to
# INCLUDING_TESTS below.
ALL_TESTS := $(sort $(patsubst tests/%.c,%,$(wildcard tests/test_*.c)))

# Tests for the application loop, split by topic.
APP_TESTS     := $(filter test_app test_app_%,$(ALL_TESTS))
# The app as those programs link it: every file of it but main.c
APP_SRCS      := $(filter-out app/main.c,$(wildcard app/*.c))
APP_TEST_SRCS := $(patsubst %,tests/%.c,$(APP_TESTS))
TEST_APP      := $(patsubst %,$(HOST_DIR)/%$(EXE),$(APP_TESTS))

# Tests that #include the source file they test (one each for the target
# adapters and main.c) rather than linking it; add new ones to this list.
INCLUDING_TESTS = test_serial_logger test_null_logger test_register_init \
                  test_analog_reader test_shift_reg_reader test_init \
                  test_timebase test_spi test_gate_output test_clock_output \
                  test_cv_output test_eeprom test_reset_cause test_main \
                  test_clock_input test_reset_input test_run_input \
                  test_watchdog test_breadcrumb \
                  test_midi_output test_null_midi_output \
                  test_midi_input test_null_midi_input \
                  test_level_output test_status_output
TEST_INCLUDING := $(patsubst %,$(HOST_DIR)/%$(EXE),$(INCLUDING_TESTS))

# Tests for the core alone, which is every other one: one program per core
# module, and the sequencer's and MIDI out's split by topic. Each is linked
# with every core source.
CORE_TESTS := $(filter-out $(APP_TESTS) $(INCLUDING_TESTS),$(ALL_TESTS))
TEST_CORE  := $(patsubst %,$(HOST_DIR)/%$(EXE),$(CORE_TESTS))

TEST_PROGRAMS = $(TEST_CORE) $(TEST_APP) $(TEST_INCLUDING)

# One recipe line that runs one test program; the blank line is what separates
# the lines when $(foreach) below strings several of these together.
define run_test
	$(call run,$1)

endef

.PHONY: test coverage
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
$(TEST_CORE): $(HOST_DIR)/%$(EXE): tests/%.c $(CORE_SRCS) $(HOST_HDRS)
	@$(call mkdir_p,$(HOST_DIR))
	$(HOST_CC) $(HOST_CFLAGS) -o $@ $< $(CORE_SRCS)

# Tests for the application loop: the real app.c and core, linked against the
# fake ports in adapters/host instead of the MCU ones. main.c is left out; the
# test program supplies main.
$(TEST_APP): $(HOST_DIR)/%$(EXE): tests/%.c $(APP_SRCS) $(CORE_SRCS) $(HOST_SRCS) $(HOST_HDRS)
	@$(call mkdir_p,$(HOST_DIR))
	$(HOST_CC) $(HOST_CFLAGS) -o $@ $< $(APP_SRCS) $(CORE_SRCS) $(HOST_SRCS)

# Coverage: rebuild the tests instrumented, run them, and report which lines
# and branches of the source they exercised. Starts from an empty folder each
# time so counts from an earlier run cannot leak in. The report fails unless
# every firmware source file is at 100% of lines and branches; see the top of
# tools/coverage/report.py for exactly what is counted.
coverage:
	@$(call rm_rf,$(BUILD_DIR)/coverage)
	$(MAKE) test COVERAGE=1
	$(PYTHON) tools/coverage/report.py $(GCOV) $(BUILD_DIR)/coverage
