# tests/ rules

Host test programs, built and run on the PC by `make test`.
Test code is outside the coding standard and may use the C library freely (`memset`, `memcpy`, `setjmp`), but it still builds with `-Wall -Wextra -Wconversion -Wshadow -Werror`.

## Which kind of test

- **Core:** one program per core module (`test_seq.c`, `test_clock.c`, `test_address.c` and so on), each linked with `core/*.c` alone.
  No ports, no adapters.
  A new one goes in `CORE_TESTS` in the Makefile.
  A module is tested fully by its own program; `test_seq.c` then checks that the sequencer uses it, not every case again.
- **App loop:** `test_app.c` links the real `app/app.c` and core against the fakes in `adapters/host/`.
- **Target adapters and `main.c`:** one program per source file, which `#include`s the `.c` file it tests.

## Host-testing a target adapter

- Have the test include `fake_atmega328p_regs.h` first.
  It uses the same include guards as every real register header, so the adapter's own `#include` of them does nothing.
  Then `#include` the adapter's `.c` file.
- `test_serial_logger.c` is the pattern.
- A new one needs its registers added to the fake header (and to its `fake_regs_reset()`), a `test_<name>.c`, and its name in `INCLUDING_TESTS` in the Makefile.
- Functions the file calls but does not define are stubs in the test.
  For a device adapter that means its driver: `test_shift_reg_reader.c` stubs `spi_transfer()` and has the stub clock the fake's 74HC165 model, `test_cv_output.c` has its stub record each byte and whether chip select was low, and the driver's own register use is tested in `test_spi.c`.
- An interrupt handler is tested by calling it: the fake header gives it an ordinary function name (`fake_timer2_compa_isr`).
- Where the hardware responds (the ADC clearing its start bit, the shift register chain presenting bits, the EEPROM storing a byte only if the write was armed), the fake models it, so the test only passes if the code drives it in the right order.
- A fake register is seen before the write the code makes to it, so a model that reacts to a write catches up at the next access (`fake_shift_sync()`, `fake_eeprom_sync()`).
  Call the sync function in the test before checking what the model did.
- A register whose flags are cleared by writing a one (`TIFR1`, `EIFR`) is a `fake_flag_reg_t`: the test sets bits in its `.flags`, the code reads and writes it as usual, and the model clears what the code wrote ones to and records the write.
  Call `fake_flags_sync()` before checking `.flags`, `.writes` or `.last_write`.
- An adapter's own static state outlives `fake_regs_reset()`.
  `test_eeprom.c` polls a leftover write through before each test; do the same for any adapter that queues work.
  Where the adapter's init is what empties its queue (`test_midi_output.c`), begin each test with `fake_regs_reset()` and then that init.
- The fake `UCSR0A` sets or clears `UDRE0` on every access, the init's own write included.
  Mask that bit out when asserting the control value the init wrote.

## Timing in the tests

- The first step is a whole step.
  A pattern started on call 1 begins step n on call 1 + n × (ticks per step); in `test_app`, each is applied and logged one tick later.
- A press of the run button is 20 ticks of the button held, and it counts on the twentieth.
  The debounce takes its first reading as it stands and the transport starts as if the button were held, so run at least one tick with the button up before a press, or it is not one.
- A pattern starts on the internal clock, and an edge takes it over.
  For one that waits on the external clock from its first tick, `test_seq.c` sets the transport's latch in the state straight after `seq_init()` (`wait_for_external()`), which is what an edge while stopped leaves; `test_app.c` has to get there with a stop, an edge and a start.
- An external edge is one tick with the edge flag set.
  Clear the flag again before the next tick, as the port does.
- In `test_app`, giving a selector position an amount takes the pot down to 0 and up again with a scan after each (`set_amount()`, 20 ticks).
  The tempo is not disturbed by it, since the pot is not on the tempo's position.
  Allow a scan (8 ticks) for any change of the panel, the pot or the LFO to be seen.
- The random numbers begin 135, 35, 70, 220, 176, 221, 238, 248 from the sequencer's seed.
  The plain engine draws none, so with it a step chance from the LFO takes them in that order.
- In `test_app`, MIDI bytes are written one tick after the core works them out, like every output, and clocks keep coming while the pattern is stopped.
  Find a message by searching for it, or compare with the clocks filtered out (`expect_midi_without_clocks()`), never by a remembered index.

## Coverage

- `make coverage` must stay at 100% of lines and branches for every `.c` in `core/`, `app/` and `adapters/target/`.
  A firmware `.c` file that no test compiles fails the check.
- If a branch truly cannot be reached from a test, tell me rather than excluding it.
- Tests check what the code does, not just that it runs: assert on the values written, the order of calls and the exact text sent.

## What these tests cannot show

Register addresses, the instructions the compiler chose, and real timing.
`make sim` covers those against an emulated chip; the board is the final check.
