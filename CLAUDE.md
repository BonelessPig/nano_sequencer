# Sequencer firmware: project rules

This is bare-metal C firmware for a microcontroller step sequencer.
These rules apply to every change.

## Architecture: ports and adapters with a pure core

- `core/` is pure logic.
  It has no vendor/HAL headers, register access, `volatile`, or ISR code, and it must compile on a PC.
- The core talks to the outside world only through headers in `ports/`, which the core owns.
- `adapters/target/` implements the ports for the MCU, and `adapters/host/` implements them for PC builds and tests.
- `app/` (main and the scheduler loop) is the only place that wires adapters to the core.
- Ports are bound at link time.
  Don't add function-pointer indirection without a clear reason.
- The main loop never blocks.
  A tick is one millisecond, counted by the timebase.
  Each tick runs in three steps: apply the outputs the previous tick computed, gather inputs, then the core step function.
  Every output is one tick late, by the same amount each time.
- ISRs do the minimum (counters, flags, ring buffers) and never call into the core.
- New features go into the core first, with tests.
  Hardware support goes into an adapter.

## Coding standard

Follow BARR-C:2018 plus the MISRA-style safety rules and line length limits listed under "Coding standard" in `README.md`; that list is the project's standard and applies to every change.
It is the same list as the "C coding standard" and "Line length" in the owner's global rules, so with those in context don't read the README for it.
Two things are particular to this project: the standard covers the firmware source (`core/`, `ports/`, `adapters/` and `app/`) and not `tests/` or `tools/sim/`, and the functions to document in a header are the port functions.

## Folder rules

A folder's `CLAUDE.md` loads by itself when a file in the folder or below it is read, so don't read it as well.
Read one yourself only before creating a file in a folder you have not read from yet.

- `core/CLAUDE.md`: what keeps the core pure.
- `ports/CLAUDE.md`: port headers and status codes.
- `adapters/target/CLAUDE.md`: registers, drivers and devices, compiler quirks.
- `adapters/host/CLAUDE.md`: the PC fakes.
- `app/CLAUDE.md`: the tick and `main`.
- `tests/CLAUDE.md`: how each kind of source file is tested, and coverage.
- `tools/misra/CLAUDE.md`: how MISRA findings are handled and how the analysis is run.
  Read it before acting on any finding.
- `tools/sim/CLAUDE.md`: the emulated board.
- `tools/sim/live/CLAUDE.md`: the live view.

`core/` and `adapters/target/` are flat, so the lessons about one part of either (an engine, MIDI out, the watchdog, the pins) are in `.claude/rules/core-*.md` and `target-*.md`, each loaded by the files it names.
The lessons about one kind of scenario on the emulated board are in `sim-*.md`, in the same way.The folder's `CLAUDE.md` lists them.

## Done means

Before calling a change done, every stage it could affect passes.
`make check` runs them all and prints one line per stage, with the full output only of a stage that fails.
Run it once, before the commit of a change to what the firmware does, not after every edit.
To run only some stages, name them to the same script: `python tools/check/run.py make coverage sim` prints the same one line each.
The names are `build debug`, `build release`, `build midi`, `test`, `coverage`, `misra` and `sim`; from Git Bash, wrap it in `cmd //c "..."` like `make`.
Use a stage's own target only when you need the output of a run that passes.
Run less where less can have changed: a refactor that `cmp` shows left all three images identical needs only `make test-quiet` (`make test` with one line of output unless a test fails), and a change to docs alone needs nothing.
Keeping token use and waiting down is a priority: do not re-run a stage nothing has touched since it last passed.
The Makefile's recipes run under `cmd.exe` on Windows; from Git Bash, call it as `cmd //c "make ..."`.

- `make`, `make CONFIG=release` and `make CONFIG=midi`: all three firmware images build.
  Any warning fails the build (`-Wall -Wextra -Wconversion -Wshadow -Werror`).
- `make test`: the host test programs pass.
- `make coverage`: 100% of lines and branches in `core/`, `app/` and `adapters/target/`.
  New firmware code comes with the tests that cover it.
  If a branch truly cannot be reached from a test, tell me rather than excluding it.
- `make -k misra`: zero findings across the four analysis runs.
- `make sim`: all three images behave on the emulated board.

`make flash` uploads to the board.
The owner runs it themselves and reports back; don't run it.

## MISRA text

- Never copy MISRA headline text into source comments, commits or docs.
  Refer to rules by number only.
- Never suppress a rule for a whole file or the whole project without asking me first.
- When you finish a task, report new MISRA findings by rule and category, along with any deviations you added.

## Working style

- Make small, incremental changes, keep the build green after each one, and make one logical change per commit.
- Don't change behavior while refactoring.
  Report suspected bugs instead of silently fixing them.
- To prove a refactor changed nothing, save `build/debug/output.hex` before editing, rebuild after, and `cmp` the two (and the release ones if the change could reach them).
  If they differ, diff the `avr-objdump -d` instruction streams with addresses stripped to see which function moved.
- Commit and push only when asked.
- Do the work in the main session.
  Don't spin off subagents, for the live view, the docs or anything else, unless I ask for them in that piece of work: each one starts cold and re-reads the project, and a run split across several used far more tokens and took far longer than doing it in one.
- Keep `README.md` and `docs/ARCHITECTURE.md` in step with the code in the same piece of work.
  ARCHITECTURE.md describes what the firmware does today and carries the MISRA status and the open items list, and quotes the totals from `docs/FIGURES.md`; `docs/ROADMAP.md` carries the plan and `docs/DECISIONS.md` the decisions taken, by number.
  Don't repeat any of those here.
- `docs/FIGURES.md` holds the sizes and the test and scenario counts, and `make figures` writes it.
  Run that once before the commit of a change to the firmware or its tests, never edit the file by hand, and copy any total that moved into the documents that quote it.
- When a change alters what the board has or does that someone could see or work (a pin, a part, a control, a mode, a log line, an output), bring the live view (`make live`, `tools/sim/live/`) up to date in the same piece of work, along with anything it needs from `tools/sim/boards/`.
  The rules of both folders load with the first file read in `tools/sim/live/`.
  Run `make sim` before calling the change done; the page cannot be checked from here, so say what the owner should look at in the browser.
- The owner wants the project minimal and lightweight, and as clean as possible against the standard: prefer actually resolving a finding over recording a deviation, and when there is a real trade-off, lay out the options with a recommendation and let them choose.

## Project facts

- Target: ATmega328P on an Arduino Nano (old bootloader), 16 MHz, avr-gcc 12.1.0, GNU Make.
  Developed on Windows; the Makefile also has macOS and Linux branches that have never been run.
- No Arduino core, no avr-libc headers or functions, no variadic functions, no `printf`, no `memset`/`memcpy`.
  The only non-project code is the startup object and a few libgcc helpers added at link time.
  `-ffreestanding` makes `<stdint.h>`, `<stdbool.h>` and `<stddef.h>` come from the compiler.
- The compiler can emit calls to `memset` or `memcpy` for large zero-initialisers and struct copies, and nothing provides them.
  In firmware code, initialise fields explicitly and avoid whole-struct assignment.
- Three build configurations, `CONFIG=debug` (default), `CONFIG=release` and `CONFIG=midi`, differing only in which logger and which MIDI output are linked: the serial logger in debug, MIDI out in midi, neither in release.
  There are no configuration macros; keep it that way and add per-configuration behaviour as another link-time choice.
  Outputs go to `build/<config>/`; objects are shared in `build/obj/`.
- Git Bash heredocs mangle backslashes in inline Python and sed scripts here.
  Write helper scripts to a file with the Write tool and run them, or use the Edit tool.
- Every text file is LF in the working tree (`.gitattributes` enforces it), on Windows too.
  Write new files with LF.
