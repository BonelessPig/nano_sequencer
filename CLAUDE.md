# Sequencer firmware: project rules

This is bare-metal C firmware for a microcontroller step sequencer. These rules apply to every change.

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

Follow BARR-C:2018 (the Barr Group Embedded C Coding Standard) for style and naming. Also follow these MISRA-style safety rules:

- Use C99 and fixed-width types from `<stdint.h>` for anything with a meaningful size.
- Don't allocate dynamically (no malloc/free) and don't use recursion. All memory is static or on the stack, with bounded sizes.
- Every `switch` has a `default`, and there's no fallthrough without a comment.
- Every `if`/`else`/`for`/`while` body uses braces.
- Each function has a single, clear purpose. Keep functions short and nesting shallow.
- Don't use implicit conversions that lose data or change signedness. Cast explicitly and make sure the cast is justified.
- Check the return value of every function that can fail, or explicitly cast it to `(void)`.
- Use no magic numbers. Use named constants or enums.
- Make anything not used outside its file `static`. Minimize globals, and mark globals shared with an ISR `volatile` and access them atomically or with interrupts masked.
- Don't use the preprocessor for logic when a `static inline` function or an enum works.
- Document every port function in its header: purpose, units, valid ranges, and whether it's ISR-safe.

## Build and checks

- The target build must compile with `-Wall -Wextra -Wconversion -Wshadow -Werror` (or the toolchain equivalent).
- Host tests must pass before a change is considered done, and `make coverage` must pass (100% of lines and branches in `core/`, `app/` and `adapters/target/`).
- Run cppcheck with the MISRA addon over `core/`, `ports/`, `adapters/` and `app/` before calling a change done. Use `make -k misra`, which runs it once per program that is actually linked (cppcheck treats its inputs as one program; the target and host adapters define the same port functions, and so do the two target loggers):

  ```
  cppcheck --addon=tools/misra/misra.json --std=c99 --enable=warning,style,performance,portability --inline-suppr --error-exitcode=1 -DF_CPU=16000000UL -I core -I ports -i adapters/target/null_logger.c core ports app adapters/target
  cppcheck --addon=tools/misra/misra.json --std=c99 --enable=warning,style,performance,portability --inline-suppr --error-exitcode=1 -DF_CPU=16000000UL -I core -I ports -i adapters/target/serial_logger.c core ports app adapters/target
  cppcheck --addon=tools/misra/misra.json --std=c99 --enable=warning,style,performance,portability --inline-suppr --error-exitcode=1 -DF_CPU=16000000UL -I core -I ports -I app -I adapters/host -I tests "--suppress=*:tests/*" core ports app/app.c adapters/host tests/test_app.c
  ```

  The first two are the debug and release firmware. The host run mirrors the `test_app` program: it leaves out `app/main.c` (the test has its own `main`) and includes `tests/test_app.c` so cppcheck can see the host fakes being called. Test code is outside the coding standard, so findings located in `tests/` are not reported. That is the one approved path-wide suppression.

  `tools/misra/misra.json` points the addon at the MISRA headlines file in `tools/misra/`, so findings come out with readable rule text and a category.

- The headlines file is copyrighted MISRA text, so it's gitignored and never committed. If it's missing, run `tools/misra/fetch_misra_headlines.sh`. If that fails, tell me rather than writing rule text yourself.
- Never copy MISRA headline text into source comments, commits or docs. Refer to rules by number only.
- cppcheck's rule ID prefix (`misra-c2012-` or `misra-c2023-`) depends on its version. Use whatever prefix the actual output shows in suppressions.

## Handling MISRA findings

MISRA C is a secondary, automated check. BARR-C plus the rules above is the standard we write to. cppcheck only implements part of MISRA, and the headlines file has only one-line rule summaries, so use judgment:

- **Mandatory**: always fix, no deviations.
- **Required**: fix by default. If fixing would make the code worse or isn't possible (common in `adapters/target/` for register access and vendor headers), add a deviation instead.
- **Advisory**: fix when it's cheap and clearly improves the code. Otherwise leave it and mention it in your summary.
- `core/` should be as close to zero findings as practical. `adapters/target/` gets more leeway, because hardware access legitimately needs things like casts to register addresses and `volatile`.
- If a headline is ambiguous and you're not sure what the rule requires, don't guess and don't make sweeping changes. Flag it to me with the rule number.
- To deviate, suppress inline and justify it on the same spot:

  ```c
  /* DEVIATION: MISRA <rule> (<category>) — <reason> */
  // cppcheck-suppress misra-c2012-<rule>
  ```

- Never suppress a rule for a whole file or the whole project without asking me first.
- When you finish a task, report new MISRA findings by rule and category, along with any deviations you added.

## Working style

- Make small, incremental changes, keep the build green after each one, and make one logical change per commit.
- Don't change behavior while refactoring. Report suspected bugs instead of silently fixing them.
- Commit and push only when asked. The owner flashes the board themselves (`make flash`) and reports back; don't run it.
- Keep `README.md` and `docs/ARCHITECTURE.md` in step with the code in the same piece of work. ARCHITECTURE.md carries the size figures, test counts, MISRA status and the open items list.
- The owner wants the project minimal and lightweight, and as clean as possible against the standard: prefer actually resolving a finding over recording a deviation, and when there is a real trade-off, lay out the options with a recommendation and let them choose.

## Project facts

- Target: ATmega328P on an Arduino Nano (old bootloader), 16 MHz, avr-gcc 12.1.0, GNU Make. Developed on Windows; the Makefile also has macOS and Linux branches that have never been run.
- No Arduino core, no avr-libc headers or functions, no variadic functions, no `printf`, no `memset`/`memcpy`. The only non-project code is the startup object and a few libgcc helpers added at link time. `-ffreestanding` makes `<stdint.h>`, `<stdbool.h>` and `<stddef.h>` come from the compiler.
- No interrupts or timers yet. Everything is polled and blocking.
- `make sim`: build both configurations and run each `output.hex` on an emulated ATmega328P (avr8js under Node) with the 74HC165 chain, tempo pot and serial capture modelled in `tools/sim/`. Scenarios are `tools/sim/scenarios/*.test.js`; `lib/` and `parts/` stay free of anything sequencer-specific. A new external part needs a model in `parts/` and its wiring in `boards/nano_sequencer.js`. The JavaScript there is test tooling, outside the C coding standard.
- What the firmware does today: each tick reads 16 four-bit step notes from eight chained 74HC165s (PD2 load, PD3 clock, PD4 data), reads the tempo pot on ADC channel 6, plays the next step of the pattern (the core advances one step per tick and wraps after 16), logs that step and its note over serial at 115200 baud (debug build only), then waits tempo/4 ms. There is no gate, CV or transport yet; new parts of that output stage go into `core/` first.
- `port_status_t` values are printed in the serial log, so never renumber them.
- Two build configurations, `CONFIG=debug` (default) and `CONFIG=release`. The only difference is which logger is linked: `serial_logger.c` or `null_logger.c`, both implementing `log_port.h` and `logger.h`. There are no configuration macros; keep it that way and add per-configuration behaviour as another link-time choice. Outputs go to `build/<config>/`; objects are shared in `build/obj/`.
- The serial rate is 115200 baud in double-speed mode, which is 2.1 % fast at 16 MHz. 250000, 500000 and 1000000 are exact. The release build never enables the USART.
- Size at the last check: debug 1294 bytes of flash and 5 bytes of static RAM; release 762 bytes and 3 bytes. `make` prints the current figures.

## Commands

- `make`: build `build/debug/output.hex` and print sizes. `make CONFIG=release` builds `build/release/output.hex`. Any warning fails the build. Build both before calling a change done.
- `make test`: build and run the ten host test programs with the PC's gcc: `test_seq`, `test_app`, and one per target adapter source plus `test_main`.
- `make coverage`: rebuild the tests with gcov instrumentation, run them, and print line and branch coverage per file. Exits 0 only if every `.c` in `core/`, `app/` and `adapters/target/` is at 100% of both. `adapters/host/` is reported but not enforced.
- `make -k misra`: all three analysis runs (debug, release, host); exits 0 only with zero findings. Pipe through `python tools/misra/summarize.py` for counts by folder, category and rule.
- `make flash`: upload with avrdude. Defaults to `COM3`; override with `PORT=`. Flashes the debug image unless `CONFIG=release` is given.
- The Makefile's recipes run under `cmd.exe` on Windows. From Git Bash, call it as `cmd //c "make ..."`.

## Things learned the hard way

- **Registers live in two files.** `adapters/target/atmega328p_regs.h` declares each register as `extern volatile`; `adapters/target/atmega328p_regs.ld` gives it its address and is passed to the linker as an extra input. A new register needs both. The USART0 registers are declared in `atmega328p_usart_regs.h` instead (same `.ld` file). Use the plain datasheet data address (for example `0x2B`). Registers at 0x20 to 0x3F need the `REG_IO_LOW` attribute or the compiler stops using `sbi`/`cbi`, which changes the shift register pulse timing. A wrong address links silently.
- **Putting the address inside the attribute does not work** on avr-gcc 12.1.0 (`io_low(0x2B)` fails with "IO definition needs an address"), which is why the linker file exists.
- **Compiler builtins need a visible prototype** or cppcheck reports rule 17.3. `delay.c` declares `__builtin_avr_delay_cycles` for that reason; avr-gcc accepts the redeclaration and still inlines it.
- **The compiler can emit calls to `memset` or `memcpy`** for large zero-initialisers and struct copies, and the project no longer provides either. In firmware code, initialise fields explicitly and avoid whole-struct assignment. Tests can use them freely.
- **cppcheck treats its inputs as one program.** That is why there are three runs, each matching a real link. The host run includes `tests/test_app.c` and leaves out `app/main.c`.
- **A macro used by only one build configuration is a rule 2.5 finding in the other.** The release run reported the USART bit positions as unused while they were in `atmega328p_regs.h`. Definitions that only one configuration's files use go in a header only those files include.
- **cppcheck does not define `__cppcheck__` when `-D` is on its command line**, so that macro cannot be used to show it different code.
- **Proving a refactor changed nothing:** build before and after and `cmp` the two `build/debug/output.hex` files (and the release ones if the change could reach them). If they differ, diff the `avr-objdump -d` instruction streams with addresses stripped to see exactly which function moved. Save the reference image before editing.
- **Host-testing a target adapter:** have the test include a fake register header that uses the same include guard as the real one, then `#include` the adapter's `.c` file (the fake claims the guards of both register headers). `tests/test_serial_logger.c` and `tests/fake_atmega328p_regs.h` are the pattern. Every target adapter and `main.c` is tested this way. A new one needs its registers added to the fake header (and to its `fake_regs_reset()`), a `tests/test_<name>.c`, and its name in `INCLUDING_TESTS` in the Makefile. Functions the file calls but does not define are stubs in the test.
- **Coverage is part of done.** `make coverage` must stay at 100% line and branch for firmware source. New firmware code comes with the tests that cover it; a firmware `.c` file with no test fails the check. If a branch truly cannot be reached from a test, tell me rather than excluding it.
- **Git Bash heredocs mangle backslashes** in inline Python and sed scripts here. Write helper scripts to a file with the Write tool and run them, or use the Edit tool.
- **`*.sh` files must stay LF** (`.gitattributes` enforces it); the repo otherwise checks out as CRLF on Windows.

## Open items

- The tempo delay is 0 to 255 ms with no floor, so at the fast end of the pot the pattern runs as fast as the loop can go (hundreds of steps per second in debug, thousands in release). The pot-to-delay mapping in the core needs a floor and a more musical range before an output stage is useful.
- A debug tick takes about 2 ms besides the tempo delay (one log line, about 1.6 ms) and a release tick under 1 ms, so the same pot position plays slightly slower in debug. A timer-based step period would remove the difference.
- PB5, PC0 and PC1 are configured but unused, and are free for the output stage. PD0 and PD1 (the UART pins) are not configured by `register_init.c`; leave them alone so the release build does not drive them.
- No crash log yet. The release build has no logging at all. A persistent error log in EEPROM is wanted and there is ample space, but the owner chose to wait until there is a watchdog and an output stage, so it can record real resets.
- The MISRA result is zero findings from cppcheck, which implements only part of MISRA C. The `io_low` attribute and the delay builtin are compiler extensions it does not flag.
