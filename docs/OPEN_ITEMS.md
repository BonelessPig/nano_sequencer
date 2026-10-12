# nano_sequencer — open items

What is known to be wrong, unfinished or unchecked on the board, item by item.
What the firmware does today is in [ARCHITECTURE.md](ARCHITECTURE.md), and the plan in [ROADMAP.md](ROADMAP.md).

The numbers are referred to from other documents, so a closed item keeps its number.

## 1. Unused pins are configured

PC1 is given a direction in `register_init.c` and never used, and PD3 is made an input with its pull-up on.

## 2. The crash log has not run on the board, and needs the reset cause

- **It records nothing unless the reset cause reaches the firmware.** With the stock old bootloader a watchdog reset never gets back to the firmware (item 15).
  Optiboot, from memory, clears the reset flags and hands the value on in a CPU register that this firmware does not read; if so, every start reads as cause 0 until the firmware takes the value from there or the board runs with no bootloader.
- **Reading it takes the debug image**, which logs the newest record at start-up and leaves the EEPROM alone.
  The other seven are only in the EEPROM, from address 16.
  Nothing clears the log but erasing the EEPROM.
- **To see it work**, use the test build of item 15: the log should read `Reset = 8` and `Crash 1 Place = 5 Count = 1`, with the count going up each time.
- Reading the log at start-up holds the debug build's first log line back by 0.5 ms, and 2.6 ms more when there is a record to report.
  The pins are not affected (DECISIONS.md, decision 111).

## 3. The timer tick has not run on the board

One tick period should be measured on the real board: the emulator's timer model is new to this project, and the board's resonator sets the real tempo accuracy.

## 4. Closed: the first step after reset was one tick short

The tick that starts the pattern no longer counts towards its second step.

## 5. `make check` under-reports static RAM

`avr-size` counts constant data under flash only, although it is also copied to RAM: it prints 382 bytes for the debug build where 966 are used, 248 for release where 316 are, and 282 for midi where 350 are.

## 6. The pattern controls have no hardware

Direction, first and last step, one-shot, scale, root and gate length are tested in the core, but nothing on the board sets them, so only the forward, looping, chromatic path is ever taken there.
One place exists for them: the four spare inputs of the selector's 74HC165, for switches. Every selector position is now in use.

## 7. A change of direction or range mid-cycle jumps

Reversing mid-pattern jumps to the matching point of the reversed cycle and does not turn round on the step it is on.
Nothing can change these controls yet; turning in place would be a change inside `address.c` only.

## 8. The shift registers on the SPI bus have not run on the board

The board has to be rewired first: load from D2 to D9, clock from D3 to D13, data from D4 to D12 through 1 kΩ.
Only the board can show that SPI mode 0 samples each bit of a real chain correctly, and that the on-board LED on D13 does no harm at 1 MHz.
If a bit comes back shifted by one place, the mode is the first thing to look at.

## 9. The gate and clock outputs have not run on the board, and have no buffer

A logic analyser or an LED and resistor on D4 and D5 is the check: at 150 BPM the clock should be a 10 Hz square wave.
Do not patch them into other equipment until the buffer stage is built: the pins have no protection.

## 10. The pitch CV has not run on the board, and is not calibrated or buffered

There is no DAC on the board yet.
In this order:

- **Measure it.** With a meter on VOUTA: 0 V for note value 1, and 83.3 mV more for each note value above it in the chromatic scale.
- **Listen for the bus.** The DAC is written every millisecond beside an analogue output.
  If that is audible, the first thing to try is writing only when the value changes, a few lines in `cv_output.c`, at the cost of the even timing.
- **Buffer it.** VOUTA has no protection and little drive.

The DAC takes a new pitch 1.6 µs before the gate rises and needs 4.5 µs to settle, so the gate leads the settled pitch by about 3 µs.
If that ever matters, the core can hold the gate back a tick.

## 11. Calibration and its storage have not run on the board

It waits on the DAC of item 10.
Then, in this order:

- **Read the start-up line.** After a power-up it should be `Reset = 1 Cal = 0`, and after pressing reset `Reset = 2 Cal = 0`.
  If both read 0, the bootloader is clearing the flags (item 2).
- **Enter the mode.** Power up with the last four steps set 10, 5, 10, 5 and the first step at 0.
  The gate should stay high, the clock output low, and the pitch CV follow the pot over 0 to 63 mV.
  Setting the first step to 1 to 4 should give about 1 to 4 V.
- **Calibrate** each point against the meter and flip the store switch off and on.
  The gate should drop for a quarter of a second and the debug build log `Cal point n = value`.
- **Check it kept.** Power off, set an ordinary pattern, power on: the line should end `Cal = 1` and the octaves measure true.

To watch for: whether the pot holds steady to the millivolt (there is no hysteresis), whether 64 mV each way is enough range, and whether the entry pattern is too awkward.
Each is a constant or two in `core/cal.c`.

## 12. The clock input, reset input and run button have not run on the board

None is wired, and the transistor stage for the two jacks is not designed in detail.
In this order:

- **The button**, which needs only a wire from D7 to ground.
  A press should log `Transport = 0` and the next `Transport = 1` then `Step 0`.
  If one press sometimes counts twice, the 20 ms in `core/debounce.c` is too short.
- **The edge flags.** That a flag is set while its interrupt is off rests on the datasheet alone.
  Touching D8 to ground should log `Transport = 2` the first time and play one step each time.
  If nothing happens, the fallback is an interrupt with a handler that sets a flag of the firmware's own.
- **The input stage.** The pin must fall when the jack rises.
  The pin's own pull-up (20 to 50 kΩ) sets how fast it rises again, which limits how fast a clock can be followed.
- **Against a real clock.** Listen for whether the 1 to 2 ms delay is audible, check all seven ratio positions, and that a pot near a boundary does not chatter.
- **Reset with a clock.** A reset sent with a clock pulse should play the first step on that pulse.
  If the master sends it a millisecond late, the wrong step plays first.

Still to look at: whether the 1 ms of low before an early step is long enough for what is patched to the clock output (one comparison in `core/seq.c` makes it longer).
Known limits: the first step after the clock starts has a 10 ms gate; the reset input has no noise canceller.

## 13. The mode switches and the engines have not run on the board

The ninth 74HC165 is not wired, so the board as it stands reads all eight mode switches as off: the plain engine.
In this order:

- **The chain.** With the ninth chip added and every switch off, the pattern should play as before.
- **The engine switches.** Input A alone should give the stage engine: the logged steps run 0 to 7 and never reach 8.
- **The stage controls**, one at a time.
  Hold is the one to look at on a scope: one gate across the stage with no gap.
- **The other engines.** The Klee engine is silent until a pattern switch is on and a stage has a value.

Whether the provisional choices sound right is the other thing to find out (DECISIONS.md, decisions 74 to 96), and whether the same "random" choices after every power-on matter in use.

## 14. The step lights have not run on the board

One thing is an assumption: that PB1 can be the 74HC165s' load line and the 74HC595s' latch clock at once.
Wire them as `SCHEMATIC.html` shows and look for one light at a time, moving with the steps and dark for the second half of each.
Anything else means the latch is catching something it should not: scope D9 and D11 across one read.
Check too that the panel still reads right with eleven chips on the clock.
A brief flash at power-on is expected, and the lights are up to a scan (8 ms) late by design.
Keep each LED to a few milliamps.

## 15. The watchdog has not run on the board, and the bootloader matters

- **Do not expect recovery with the stock old bootloader.** It leaves the watchdog running at 16 ms, so after a watchdog reset the board resets inside the bootloader until the power is cycled.
  Burn Optiboot first (DECISIONS.md, decision 3), which also changes the upload speed in the Makefile.
- **To see it work** there is no way from the panel to make the firmware hang.
  A test build that stops kicking while the run button is held would do it.
- **The reset cause** may read 0 if the bootloader clears the flags first, and then no crash is recorded (item 2).

## 16. MIDI out has not run on the board, and its circuit is not built or verified

- **The circuit** is taken to be the usual one: DIN pin 4 through 220 Ω to +5 V, pin 5 through 220 Ω from D1, pin 2 to ground.
  That is from memory of the MIDI specification and not checked against it.
- **The line before the USART has it.** PD1 floats from reset until `midi_output_init()`.
  If a receiver takes that as a byte, try a pull-up.
- **The USB serial chip** shares D1, so the PC's port shows the MIDI bytes as noise.
- **A receiver** on channel 1 should play the pattern with note value 1 as C1; a monitor should show six clocks a step and no Note On without its Note Off.

Known limits: there is no All Notes Off, so a reset in the middle of a note leaves it sounding (a scenario in `midi.test.js` holds this, to be turned round when it changes); a start is always from the first step; and once an external edge has come there is no clock while stopped.

## 17. MIDI in is not built

The second piece of the roadmap's phase 10.
The receiver is already enabled in the midi image, and nothing reads it.

## 18. A step short on a very fast external clock

Seen on the emulated board on 2026-10-11 and not looked into: with edges 8 ms apart and a multiply of four (a step every 2 ms), the pins show 196 steps where 197 are due, while the MIDI clocks are exact.
At 9 ms and slower the count is right.
It may be the rule that holds a step back a tick while the clock output is still high.
No scenario holds it yet.

## 19. The modulation input has no input stage, and has not run on the board

The stage has to scale and shift an LFO (±5 V is usual) into 0 V to AVcc with 0 V at the jack landing on half of AVcc, hold the pin there with nothing plugged in, and keep it within the supply.
Its centre has to be within 16 counts (about 78 mV) of half of AVcc, or the amounts move things with no LFO present.
A failed read of the LFO is taken as no modulation and is not logged.

## 20. The selector, the pot's amounts and the level light have not run on the board

The tenth 74HC165 is not wired; until it is the selector reads 0 and the pot is the tempo.
Only the board can show whether a coded switch reads a third position between detents (harmless to the amounts), whether the pot is steady enough for the pickup's window of 8 counts, and whether the level light is even in brightness across its range.

The profile button is on that chip too, on its fourth input, and has not run either.
On the board, look at: that a click, and holds of 1, 5 and 10 s, do what they should with nothing to show which was reached (the status light is still to come, and until then only the debug log says); that a save in the middle of a pattern is not heard (on the emulated board no step moves through the 55 ms it takes); and that a profile comes back after the power has been off.
A save rewrites up to 16 bytes of an EEPROM rated for 100,000 writes a byte, from memory; a byte that already holds its value is not written.

## 21. The first step after a crash is 0.2 ms late in the debug build

After a watchdog reset the debug build's first gate rises about 0.23 ms late (emulated board): start-up has the crash to read and report, and the first pass of the loop runs into the second tick.
Only that one step, in that build, after a crash.

## 22. Going to the external clock at power-on plays a step first

The pattern starts on the internal clock, so with a master already running step 0 begins at once and the first edge begins step 1.
A stop and a start puts the two in step.
A jack with a switch contact on a spare pin (PD3) would tell at power-on that a clock is coming.

## 23. Closed: the figures were kept by hand

`make figures` now measures them and writes [FIGURES.md](FIGURES.md).
The totals in ARCHITECTURE.md's "At a glance" and build table and in the README are still copies of it: bring them up to date from that file when it changes.
