# app/ rules

The only code that knows about both the core and the ports.

- `app_run_once()` is one pass of the main loop and never waits.
  Every pass polls the log, the MIDI output and the storage (`log_poll()`, `midi_output_poll()`, `nv_poll()`), and a pass on which the timebase reports no tick does only that.
  Otherwise it runs one tick in three steps: apply the outputs the previous tick computed, gather inputs through the ports into one snapshot, call the core.
  Outputs go first so they change at the same point in every tick.
- Within the apply step, the outputs come before the log: the pitch CV, then the gate, then the clock output, all written on every tick with what the core gave, whether or not it changed.
  Writing all of them every time is what keeps the gate and clock edges at the same point in every tick.
  The pitch goes ahead of the gate so it is in place when the note starts, and the log stays last.
  The step lights are given after the clock output, on every tick too; that call only keeps the value, and the lights change at the next panel scan.
- The core's `cv` value goes to the `cv` port as millivolts unchanged; the calibration table the app gives the core is what makes the two the same thing.
- `watchdog_kick()` is called once in a tick, after the outputs are applied, and never on a pass with no tick: a tick that stops coming has to trip the watchdog too.
  Nothing else kicks it.
- Keep decisions in the core; this file only moves data, schedules the input scan and reports failed reads.
  The elapsed tick count goes to the core as an ordinary input.
- The mode switches come with the panel read, as its ninth byte, and go to the core undecoded: which engine they mean is the core's business.
- The panel and the pot are read every `INPUT_SCAN_PERIOD_TICKS` (8 ms), not every tick; the core gets the latest snapshot in between.
  Failed reads are reported once per step, with the step, not once per scan, and so is a failed pitch CV write, although it happens every tick.
- The three transport inputs (clock edge, reset edge, run button) are read on every tick, ahead of the scan, and go to the core as read: an edge lasts one tick, and the button is debounced in the core.
  Do not move them to the 8 ms scan: an edge would wait up to 8 ms, and the debounce counts ticks.
  MIDI in is read with them, calibration mode included so that nothing piles up: eight bytes at most (`MIDI_IN_BYTES_PER_TICK`), which `midi_in_read()` turns into three flags for the sequencer's inputs.
  The buffer is a static inside `read_midi_in()`: at file scope it is a rule 8.9 finding, and on the stack it would be set to zeroes every tick.
- A change of transport is logged in the apply step, after the outputs and ahead of the step, so that a start reads `Transport = 1` and then `Step 0`.
- Core inputs the board has no hardware for (direction, range, one-shot, scale, root, gate length) are set once in `set_fixed_controls()`.
  When one gets a control, read it through a port in the scan and take it out of that function.
  The pitch calibration table is set there too, to the ideal one, and `load_calibration()` then puts the stored one over it if the record is valid.
- The core step is `cal_tick()` and then, unless it says calibration mode is active, `seq_tick()`.
  In the mode the app copies the pitch CV and the gate from the calibration outputs and leaves the rest of the sequencer outputs as init set them (no step, clock low), so the apply step needs no second path.
- The pot is read into the app's own snapshot, not the sequencer's inputs.
  Calibration mode takes it as read.
  Otherwise `bank_tick()` runs ahead of `seq_tick()`, with the selector as `panel_selector_value()` gives it, and the sequencer's tempo reading is the bank's tempo slot, always valid.
  The slots are named by `SLOT_*` at the top of `app.c`: the tempo is position 0, which is what a board with no selector wired reads, and positions 1 to 5 are how much of the LFO goes to the note, the gate length, the chance of a step dropping out, the step played and the amplitude, position 6 is the envelope's time, which goes to `env_tick()` and not to the sequencer, and position 7 is how much of the LFO goes to the vibrato.
  Their amounts are copied into the sequencer's inputs on every tick.
- The modulation input is read into the app's own snapshot on every tick but a scan's, and the LFO rate pot in the scan, after the pot: a scan's tick has the rate pot's conversion where the others have the modulation input's, so no tick is longer than it was.
  `lfo_tick()` runs after the bank and ahead of `seq_tick()`, and what it gives is the sequencer's LFO, always valid; it is not called in calibration mode.
  A read of either that fails is not logged: the core takes a missing modulation input as one at its centre and keeps the last rate.
- The level light is given `bank_level()`, worked out with the bank and written in the apply step of the next tick, after the step lights, on every tick.
  In calibration mode it stays at 0.
- The envelope is `env_tick()`, called after `seq_tick()` and MIDI out with the gate and the amplitude the sequencer gave, and not called in calibration mode, where its level stays 0.
  Its level is written to the envelope port in the apply step of the next tick, on every tick, after the clock output: the second DAC frame must not come between the pitch and the gate, or every gate edge moves.
  A failed write of it is logged with the step as a failed pitch write is, under the same error, and only if the pitch's own write went.
- MIDI out is `midi_out_tick()`, called after `seq_tick()` with plain values from the sequencer's outputs, and not called in calibration mode, which sends nothing.
  Its bytes are written in the apply step of the next tick, after the step lights and ahead of the log, on every tick, a count of 0 included, so the apply step takes the same path every time.
- The MIDI output is polled a second time on a pass with a tick, between the gather and the core, so that a byte can leave during the tick's work.
  Without it the midi image at a step every 5 ms gave up Note Ons once MIDI in had added 9 µs to a tick (decision 202).
  Anything that lengthens a tick moves where in a loop pass the next tick falls, by up to a pass (6 µs): a scenario bound tighter than that fails for no other reason.
- The MIDI output's room is read in the gather step on every tick.
  The room read on one tick is still good when that tick's bytes are written on the next, because the queue only drains in between.
  Keep the write ahead of the read within a tick, or that stops being true.
- The profiles are in `app_profile.c`, split from `app.c` by topic, with its own statics; `app.c` calls it in three places and hands it the bank.
  `app_profile_run()` is called after the bank and ahead of the LFO, and not in calibration mode; `app_profile_apply()` in the apply step of the next tick, after a calibration save and ahead of the log, on every tick.
  Nothing of it comes before the MIDI bytes are queued, so no output moves for a recall or a save.
  A recall or a clear that meets `ERR_BUSY` is kept and tried again on the next tick; a save is not.
  The load at power-on is the core's doing: `profile_tick()` gives `PROFILE_LOAD` on the first tick the panel is valid.
  Do not read the panel in start-up for it: that was tried, and it moved the first tick on the emulated board.
- The status light is in `app_profile.c` too: `blink_tick()` in `app_profile_run()`, written first thing in `app_profile_apply()` on every tick, and dark in calibration mode, where the first is not called.
  Which profiles hold anything is a bit each, read at power-on a byte a tick for eight ticks, ahead of the power-on recall and never on the same tick as it.
  Keep a whole record's read off the tick a step begins on and the two after: the midi image's bytes are leaving then, and a tenth of a millisecond more in the pass holds one up.
- A new file in `app/` is linked into the `test_app` programs and the `misra-host` run by itself (`APP_SRCS` in `tests/tests.mk`), `main.c` excepted.
- A stored calibration point is saved in the apply step of the next tick, after the three outputs: the whole record is encoded and handed to `nv_write()`, which only starts it.
- Start-up reads the reset cause and the calibration record once, after `platform_init()` has succeeded, and logs both in one line.
  A record that is not valid is not an error.
- Start-up then sees to the crash log: it reads the forty bytes, gives the core the reset cause and the breadcrumb as it was left, starts the write of the one record the core changed, and logs the newest record if there is one.
  The core decides whether the start was a crash; the app does not look at the cause.
  Read the breadcrumb before writing it.
- `breadcrumb_write()` marks each part of the tick as it begins (apply, gather, core) and the end of it, and start-up once.
  The `PLACE_*` numbers are stored in the EEPROM and printed: never renumber one, and give a new place a new number.
  A pass with no tick writes nothing.
- Anything added to start-up delays the debug build's first log lines, since nothing is sent until the loop runs (DECISIONS.md, decision 111).
- A port function that waits (`log_flush`) is for the path where the loop is not going to run.
- A port's status goes to the core as a valid flag, and to the log as a status code.
  The core never sees a `port_status_t`.
- No register access and no MCU headers here.
  `app.c` is also linked into the `test_app` host program against the fakes.
- `main.c` is init, then the endless loop.
  It is left out of `test_app` and of the `misra-host` run (the test has its own `main`), and is tested by `tests/test_main.c`, which includes it with `main` renamed.
