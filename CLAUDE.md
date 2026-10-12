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

BARR-C:2018 plus MISRA-style safety rules, for the firmware source.
The rules are in `.claude/rules/c-standard.md`, loaded with any `.c` or `.h` file.

## Folder rules

A folder's `CLAUDE.md` loads by itself when a file in the folder or below it is read, so don't read it as well.
Read one yourself only before creating a file in a folder you have not read from yet.

Each of `core/`, `ports/`, `adapters/target/`, `adapters/host/`, `app/`, `tests/`, `tools/misra/`, `tools/sim/` and `tools/sim/live/` has one.
Read `tools/misra/CLAUDE.md` before acting on any MISRA finding.

`core/` and `adapters/target/` are flat, so the lessons about one part of either (an engine, MIDI out, the watchdog, the pins) are in `.claude/rules/core-*.md` and `target-*.md`, each loaded by the files it names.
The lessons about one kind of scenario on the emulated board are in `sim-*.md`, in the same way.
The folder's `CLAUDE.md` lists them.

## Done means

Before calling a change to code done, every stage of `make check` it could affect passes.
How to run it, and what a change to the firmware brings with it (the refactor proof, the figures, the live view), is in `.claude/rules/checks.md`, loaded with any source, test or tooling file.
A change to docs alone needs nothing.

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
- Commit and push only when asked.
- Do the work in the main session.
  Don't spin off subagents, for the live view, the docs or anything else, unless I ask for them in that piece of work: each one starts cold and re-reads the project, and a run split across several used far more tokens and took far longer than doing it in one.
- Keep `README.md` and `docs/ARCHITECTURE.md` in step with the code in the same piece of work.
  ARCHITECTURE.md describes what the firmware does today and carries the MISRA status, and quotes the totals from `docs/FIGURES.md`; `docs/OPEN_ITEMS.md` carries the open items, by number, so a change to one of those need not open ARCHITECTURE.md; `docs/ROADMAP.md` carries the plan and `docs/DECISIONS.md` the decisions taken, by number.
  Don't repeat any of those here.
- Split a file by topic before it passes its limit: about 25,000 characters for source, tests and tooling, 6,000 for a `CLAUDE.md` or a rules file.
  The `sizes` line of `make check` names the files that are over (`tools/check/sizes.py` has the limits and the exceptions); say so when one appears there.
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
- The Makefile's recipes run under `cmd.exe` on Windows; from Git Bash, call it as `cmd //c "make ..."`.
- Git Bash heredocs mangle backslashes in inline Python and sed scripts here.
  Write helper scripts to a file with the Write tool and run them, or use the Edit tool.
- Every text file is LF in the working tree (`.gitattributes` enforces it), on Windows too.
  Write new files with LF.
