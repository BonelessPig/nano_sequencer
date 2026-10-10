# Sequencer firmware: project rules

This is bare-metal C firmware for a microcontroller step sequencer. These rules apply to every change. Each folder listed under "Folder rules" has its own `CLAUDE.md` with the rules and lessons specific to it.

## Architecture: ports and adapters with a pure core

- `core/` is pure logic. It has no vendor/HAL headers, register access, `volatile`, or ISR code, and it must compile on a PC.
- The core talks to the outside world only through headers in `ports/`, which the core owns.
- `adapters/target/` implements the ports for the MCU, and `adapters/host/` implements them for PC builds and tests.
- `app/` (main and the scheduler loop) is the only place that wires adapters to the core.
- Ports are bound at link time. Don't add function-pointer indirection without a clear reason.
- Each tick runs in three steps: gather inputs, then the core step function, then apply outputs.
- ISRs do the minimum (counters, flags, ring buffers) and never call into the core.
- New features go into the core first, with tests. Hardware support goes into an adapter.

## Coding standard

Follow BARR-C:2018 plus the MISRA-style safety rules and line length limits listed under "Coding standard" in `README.md`; that list is the project's standard and applies to every change.

## Folder rules

Read a folder's `CLAUDE.md` before adding a file to it or changing one in it.

- `core/CLAUDE.md`: what keeps the core pure.
- `ports/CLAUDE.md`: port headers and status codes.
- `adapters/target/CLAUDE.md`: registers, the two loggers, pins, compiler quirks.
- `adapters/host/CLAUDE.md`: the PC fakes.
- `app/CLAUDE.md`: the tick and `main`.
- `tests/CLAUDE.md`: how each kind of source file is tested, and coverage.
- `tools/misra/CLAUDE.md`: how MISRA findings are handled and how the analysis is run. Read it before acting on any finding.
- `tools/sim/CLAUDE.md`: the emulated board.

## Done means

Before calling a change done, all of these pass. `make check` runs them all and prints one line per stage, with the full output only of a stage that fails; use it for the final run, and a stage's own target when you need its output. The Makefile's recipes run under `cmd.exe` on Windows; from Git Bash, call it as `cmd //c "make ..."`.

- `make` and `make CONFIG=release`: both firmware images build. Any warning fails the build (`-Wall -Wextra -Wconversion -Wshadow -Werror`).
- `make test`: the host test programs pass.
- `make coverage`: 100% of lines and branches in `core/`, `app/` and `adapters/target/`. New firmware code comes with the tests that cover it. If a branch truly cannot be reached from a test, tell me rather than excluding it.
- `make -k misra`: zero findings across the three analysis runs.
- `make sim`: both images behave on the emulated board.

`make flash` uploads to the board. The owner runs it themselves and reports back; don't run it.

## MISRA text

- Never copy MISRA headline text into source comments, commits or docs. Refer to rules by number only.
- Never suppress a rule for a whole file or the whole project without asking me first.
- When you finish a task, report new MISRA findings by rule and category, along with any deviations you added.

## Working style

- Make small, incremental changes, keep the build green after each one, and make one logical change per commit.
- Don't change behavior while refactoring. Report suspected bugs instead of silently fixing them.
- To prove a refactor changed nothing, save `build/debug/output.hex` before editing, rebuild after, and `cmp` the two (and the release ones if the change could reach them). If they differ, diff the `avr-objdump -d` instruction streams with addresses stripped to see which function moved.
- Commit and push only when asked.
- Keep `README.md` and `docs/ARCHITECTURE.md` in step with the code in the same piece of work. ARCHITECTURE.md describes what the firmware does today and carries the size figures, test counts, MISRA status and the open items list; `docs/ROADMAP.md` carries the plan and the decisions taken. Don't repeat any of those here.
- The owner wants the project minimal and lightweight, and as clean as possible against the standard: prefer actually resolving a finding over recording a deviation, and when there is a real trade-off, lay out the options with a recommendation and let them choose.

## Project facts

- Target: ATmega328P on an Arduino Nano (old bootloader), 16 MHz, avr-gcc 12.1.0, GNU Make. Developed on Windows; the Makefile also has macOS and Linux branches that have never been run.
- No Arduino core, no avr-libc headers or functions, no variadic functions, no `printf`, no `memset`/`memcpy`. The only non-project code is the startup object and a few libgcc helpers added at link time. `-ffreestanding` makes `<stdint.h>`, `<stdbool.h>` and `<stddef.h>` come from the compiler.
- The compiler can emit calls to `memset` or `memcpy` for large zero-initialisers and struct copies, and nothing provides them. In firmware code, initialise fields explicitly and avoid whole-struct assignment.
- Two build configurations, `CONFIG=debug` (default) and `CONFIG=release`, differing only in which logger is linked. There are no configuration macros; keep it that way and add per-configuration behaviour as another link-time choice. Outputs go to `build/<config>/`; objects are shared in `build/obj/`.
- Git Bash heredocs mangle backslashes in inline Python and sed scripts here. Write helper scripts to a file with the Write tool and run them, or use the Edit tool.
- `*.sh` files must stay LF (`.gitattributes` enforces it); the repo otherwise checks out as CRLF on Windows.
