# tests/ rules

Host test programs, built and run on the PC by `make test`. Test code is outside the coding standard and may use the C library freely (`memset`, `memcpy`, `setjmp`), but it still builds with `-Wall -Wextra -Wconversion -Wshadow -Werror`.

## Which kind of test

- **Core:** one program per core module (`test_seq.c`, `test_clock.c`, `test_address.c` and so on), each linked with `core/*.c` alone. No ports, no adapters. A new one goes in `CORE_TESTS` in the Makefile. A module is tested fully by its own program; `test_seq.c` then checks that the sequencer uses it, not every case again.
- **App loop:** `test_app.c` links the real `app/app.c` and core against the fakes in `adapters/host/`.
- **Target adapters and `main.c`:** one program per source file, which `#include`s the `.c` file it tests.

## Host-testing a target adapter

- Have the test include `fake_atmega328p_regs.h` first. It uses the same include guards as all three real register headers, so the adapter's own `#include` of them does nothing. Then `#include` the adapter's `.c` file.
- `test_serial_logger.c` is the pattern.
- A new one needs its registers added to the fake header (and to its `fake_regs_reset()`), a `test_<name>.c`, and its name in `INCLUDING_TESTS` in the Makefile.
- Functions the file calls but does not define are stubs in the test.
- An interrupt handler is tested by calling it: the fake header gives it an ordinary function name (`fake_timer2_compa_isr`).
- Where the hardware responds (the ADC clearing its start bit, the shift register chain presenting bits), the fake models it, so the test only passes if the code drives it in the right order.

## Coverage

- `make coverage` must stay at 100% of lines and branches for every `.c` in `core/`, `app/` and `adapters/target/`. A firmware `.c` file that no test compiles fails the check.
- If a branch truly cannot be reached from a test, tell me rather than excluding it.
- Tests check what the code does, not just that it runs: assert on the values written, the order of calls and the exact text sent.

## What these tests cannot show

Register addresses, the instructions the compiler chose, and real timing. `make sim` covers those against an emulated chip; the board is the final check.
