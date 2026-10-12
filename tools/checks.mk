# The checks other than the host tests: `make misra`, `make sim`, `make
# live`, `make check`, `make file-sizes`, `make figures` and `make
# test-quiet`. Included by the Makefile after tests/tests.mk, and uses what
# both define.

# ---- Static analysis --------------------------------------------------------
#
# Static analysis: cppcheck plus its MISRA addon (see tools/misra/CLAUDE.md for how
# findings are handled). tools/misra/misra.json points the addon at the rule headlines
# file, which is not in the repo; if it is missing, run
# tools/misra/fetch_misra_headlines.sh first. Exits non-zero if anything is found.
#
# The analysis runs four times, once per program that is actually linked,
# because cppcheck treats everything it is given as one program. The target and
# host adapters each define the same port functions, as do the two target
# loggers and the two target MIDI outputs (only one of each is ever linked),
# and analysing them together would report every such function as defined
# twice.
#
# Each run analyses the same files that are linked together for that build:
#   misra-debug    core, ports, all of app (including main.c), adapters/target
#                    without null_logger.c and midi_output.c
#   misra-release  the same without serial_logger.c and midi_output.c
#   misra-midi     the same without serial_logger.c and null_midi_output.c
#   misra-host     core, ports, app/ but main.c, adapters/host, tests/test_app*.c
# The host run mirrors the test_app programs. It leaves out app/main.c because
# the tests supply their own main, and it includes the test files only so
# cppcheck can see that the host fakes' control functions are called from
# another file. Test code is not held to the coding standard, so findings
# located in tests/ are not reported (MISRA_HOST_SCOPE).
#   make misra         All runs; stops at the first one that reports findings
#   make -k misra      All runs even if an earlier one reports findings
#   -i <file>       Leave that file out of the run
#   MISRA_INCLUDES  Where cppcheck looks for the project's own headers
#   -DF_CPU=...     Same define the compiler gets; without it the headers hit
#                     their #error and cppcheck skips the code
#
# cppcheck is only needed here, not for building or flashing. Its MISRA addon
# is a Python script, so python must be on PATH as well.
CPPCHECK = cppcheck

MISRA_INCLUDES   = -I core -I ports
MISRA_FLAGS      = --addon=tools/misra/misra.json --std=c99 \
                   --enable=warning,style,performance,portability \
                   --inline-suppr --error-exitcode=1 -DF_CPU=$(F_CPU)
MISRA_HOST_SCOPE = -I app -I adapters/host -I tests "--suppress=*:tests/*"

.PHONY: misra misra-debug misra-release misra-midi misra-host
misra: misra-debug misra-release misra-midi misra-host

# The three firmware runs share a recipe. MISRA_SKIP names the files of
# adapters/target that the run's build does not link.
misra-debug:   MISRA_SKIP = null_logger midi_output
misra-release: MISRA_SKIP = serial_logger midi_output
misra-midi:    MISRA_SKIP = serial_logger null_midi_output
MISRA_SKIP_FLAGS = $(patsubst %,-i adapters/target/%.c,$(MISRA_SKIP))

misra-debug misra-release misra-midi:
	$(CPPCHECK) $(MISRA_FLAGS) $(MISRA_INCLUDES) $(MISRA_SKIP_FLAGS) core ports app adapters/target

misra-host:
	$(CPPCHECK) $(MISRA_FLAGS) $(MISRA_INCLUDES) $(MISRA_HOST_SCOPE) \
	    core ports $(APP_SRCS) adapters/host $(APP_TEST_SRCS)

# ---- Emulation --------------------------------------------------------------
#
# `make sim` runs the firmware images, exactly as they would be flashed, on an
# emulated ATmega328P (the avr8js library, under Node) with the board's
# external parts modelled in tools/sim/: the 74HC165 chain, the MCP4822 DAC,
# the tempo pot and a serial capture, and with the chip's EEPROM to store to.
# The scenarios in tools/sim/scenarios/ assert on what the firmware logs, the
# pulses it sends, what it stores and how long a step takes, counted in CPU
# cycles. Nothing here touches the board.
#
# The first run downloads avr8js into tools/sim/node_modules (needs a network
# connection once); after that it only reinstalls if package-lock.json changes.
NODE ?= node
NPM  ?= npm
SIM_DIR     = tools/sim
SIM_MODULES = $(SIM_DIR)/node_modules/.package-lock.json

.PHONY: sim
sim: $(SIM_MODULES) images
	$(NODE) --test --test-reporter=spec "$(SIM_DIR)/scenarios/*.test.js"

# `make live` runs one image on the same emulated board at the pace of a real
# chip and serves a page to watch it and work its panel from: open the address
# it prints in a browser, and stop it with Ctrl+C. It checks nothing, so it is
# not part of `make check`. LIVE_PORT picks the port: make live LIVE_PORT=9000
LIVE_PORT ?= 8165

.PHONY: live
live: $(SIM_MODULES) images
	$(NODE) $(SIM_DIR)/live/server.js $(LIVE_PORT)

$(SIM_MODULES): $(SIM_DIR)/package-lock.json
	$(NPM) --prefix $(SIM_DIR) ci

# ---- Everything at once -----------------------------------------------------
#
# `make check` runs every check that a change must pass: the firmware builds,
# the host tests, coverage, the MISRA analysis and the emulated board. It
# prints one line per stage, and the full output only of a stage that fails.
# All stages run even if an earlier one fails; it exits non-zero if any did.
# tools/check/run.py has the list of stages. Run a stage's own target (for
# example `make coverage`) to see its full output when it passes.
.PHONY: check
check:
	@$(PYTHON) tools/check/run.py $(MAKE)

# `make file-sizes` lists the tracked source and rules files that have grown
# past the size the rules allow, in one line. It is a stage of `make
# check` and never fails. tools/check/sizes.py has the limits.
.PHONY: file-sizes
file-sizes:
	@$(PYTHON) tools/check/sizes.py

# `make figures` measures what the documents quote (flash and RAM by source
# file, the host test counts, coverage and the scenario counts) and writes it
# to docs/FIGURES.md. It runs the coverage build and the emulated board, so it
# takes about as long as those two. tools/figures/figures.py has the details.
.PHONY: figures
figures: $(SIM_MODULES) images
	@$(PYTHON) tools/figures/figures.py "$(MAKE)" "$(NODE)" docs/FIGURES.md

# `make test-quiet` is `make test` through the same script: one line if every
# test passes, and the full output if one fails.
.PHONY: test-quiet
test-quiet:
	@$(PYTHON) tools/check/run.py $(MAKE) test
