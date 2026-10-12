---
paths:
  - "**/*.c"
  - "**/*.h"
  - "**/*.ld"
  - "**/*.js"
  - "**/*.mjs"
  - "**/*.py"
  - "**/*.mk"
  - "Makefile"
---

# Checking a change

Loaded with any source, test or tooling file, on top of the root `CLAUDE.md`.

## Done means

Before calling a change done, every stage it could affect passes.
`make check` runs them all and prints one line per stage, with the full output only of a stage that fails.
Run it once, before the commit of a change to what the firmware does, not after every edit.
To run only some stages, name them to the same script: `python tools/check/run.py make coverage sim` prints the same one line each.
The names are `build debug`, `build release`, `build midi`, `test`, `coverage`, `misra`, `sim` and `sizes`; from Git Bash, wrap it in `cmd //c "..."` like `make`.
Use a stage's own target only when you need the output of a run that passes.
Run less where less can have changed: a refactor that `cmp` shows left all three images identical needs only `make test-quiet` (`make test` with one line of output unless a test fails), and a change to docs alone needs nothing.
Keeping token use and waiting down is a priority: do not re-run a stage nothing has touched since it last passed.

- `make`, `make CONFIG=release` and `make CONFIG=midi`: all three firmware images build.
  Any warning fails the build (`-Wall -Wextra -Wconversion -Wshadow -Werror`).
- `make test`: the host test programs pass.
- `make coverage`: 100% of lines and branches in `core/`, `app/` and `adapters/target/`.
  New firmware code comes with the tests that cover it.
  If a branch truly cannot be reached from a test, tell me rather than excluding it.
- `make -k misra`: zero findings across the four analysis runs.
- `make sim`: all three images behave on the emulated board.

## What a change brings with it

- To prove a refactor changed nothing, save `build/debug/output.hex` before editing, rebuild after, and `cmp` the two (and the release ones if the change could reach them).
  If they differ, diff the `avr-objdump -d` instruction streams with addresses stripped to see which function moved.
- `docs/FIGURES.md` holds the sizes and the test and scenario counts, and `make figures` writes it.
  Run that once before the commit of a change to the firmware or its tests, never edit the file by hand, and copy any total that moved into the documents that quote it.
- When a change alters what the board has or does that someone could see or work (a pin, a part, a control, a mode, a log line, an output), bring the live view (`make live`, `tools/sim/live/`) up to date in the same piece of work, along with anything it needs from `tools/sim/boards/`.
  The rules of both folders load with the first file read in `tools/sim/live/`.
  Run `make sim` before calling the change done; the page cannot be checked from here, so say what the owner should look at in the browser.
