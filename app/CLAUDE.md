# app/ rules

The only code that knows about both the core and the ports.

- `app_run_once()` is one pass of the main loop and never waits. It asks the timebase how many ticks have passed; if none, it only polls the log. Otherwise it runs one tick in three steps: apply the outputs the previous tick computed, gather inputs through the ports into one snapshot, call the core. Outputs go first so they change at the same point in every tick.
- Within the apply step, the pins come before the log: the gate, then the clock output, written on every tick with the level the core gave, whether or not it changed. Anything timing-critical added later (the pitch CV) goes ahead of the gate, and the log stays last.
- Keep decisions in the core; this file only moves data, schedules the input scan and reports failed reads. The elapsed tick count goes to the core as an ordinary input.
- The panel and the tempo control are read every `INPUT_SCAN_PERIOD_TICKS` (8 ms), not every tick; the core gets the latest snapshot in between. Failed reads are reported once per step, with the step, not once per scan.
- Core inputs the board has no hardware for (direction, range, one-shot, reset, scale, root, gate length) are set once in `set_fixed_controls()`. When one gets a control, read it through a port in the scan and take it out of that function.
- Nothing in the loop may block. A port function that waits (`log_flush`) is for the path where the loop is not going to run.
- A port's status goes to the core as a valid flag, and to the log as a status code. The core never sees a `port_status_t`.
- No register access and no MCU headers here. `app.c` is also linked into the `test_app` host program against the fakes.
- `main.c` is init, then the endless loop. It is left out of `test_app` and of the `misra-host` run (the test has its own `main`), and is tested by `tests/test_main.c`, which includes it with `main` renamed.
- Both files must stay at 100% of lines and branches.
