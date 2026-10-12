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
- A change of transport is logged in the apply step, after the outputs and ahead of the step, so that a start reads `Transport = 1` and then `Step 0`.
- Core inputs the board has no hardware for (direction, range, one-shot, scale, root, gate length) are set once in `set_fixed_controls()`.
  When one gets a control, read it through a port in the scan and take it out of that function.
  The pitch calibration table is set there too, to the ideal one, and `load_calibration()` then puts the stored one over it if the record is valid.
- The core step is `cal_tick()` and then, unless it says calibration mode is active, `seq_tick()`.
  In the mode the app copies the pitch CV and the gate from the calibration outputs and leaves the rest of the sequencer outputs as init set them (no step, clock low), so the apply step needs no second path.
- The pot is read into the app's own snapshot, not the sequencer's inputs.
  Calibration mode takes it as read.
  Otherwise `bank_tick()` runs ahead of `seq_tick()`, with the selector as `panel_selector_value()` gives it, and the sequencer's tempo reading is the bank's tempo slot, always valid.
  The slots are named by `SLOT_*` at the top of `app.c`: the tempo is position 0, which is what a board with no selector wired reads, and positions 1 to 3 are how much of the LFO goes to the note, the gate length and the chance of a step dropping out.
  Their amounts are copied into the sequencer's inputs on every tick.
- The LFO is read in the scan with the panel and the pot, straight into the sequencer's inputs with its valid flag.
  A read that fails is not logged: the core takes it as an LFO at its centre.
- The level light is given `bank_level()`, worked out with the bank and written in the apply step of the next tick, after the step lights, on every tick.
  In calibration mode it stays at 0.
- MIDI out is `midi_out_tick()`, called after `seq_tick()` with plain values from the sequencer's outputs, and not called in calibration mode, which sends nothing.
  Its bytes are written in the apply step of the next tick, after the step lights and ahead of the log, on every tick, a count of 0 included, so the apply step takes the same path every time.
- The MIDI output's room is read in the gather step on every tick.
  The room read on one tick is still good when that tick's bytes are written on the next, because the queue only drains in between.
  Keep the write ahead of the read within a tick, or that stops being true.
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
