# adapters/host/ rules

The ports faked for the PC: tests script what the input ports return and read back a record of every output-port call, in order.

- `host_ports.c` implements every function in `ports/`. A new port function needs its fake here in the same change, or `test_app` will not link.
- An output written on every tick (the gate, the clock output, the pitch CV) is not put in the event record, which would fill in 64 ticks. The fake keeps its level, or the voltage last written successfully, and counts rising edges or writes; `host_event_count_at_gate_rise()` is what shows whether the gate or the log came first, and `host_cv_millivolts_at_gate_rise()` whether the pitch was in place before the gate.
- The non-volatile storage is a small array that `host_reset()` erases and `app_init()` does not, so a test powers the app off and on by calling `app_init()` again. A write lands at once; there is no write time to poll out. `host_set_nv_bytes()` puts a record there beforehand.
- `log_startup()` is counted and its arguments kept, but it is not put in the event record, so that the record of every run still begins with its first step.
- The clock and reset inputs are each a pending edge: `host_pulse_clock_input()` sets it, and the next read takes it and forgets it, as the hardware's flag does. The run button and the sync switch are levels (`host_set_run_button()`, `host_set_sync_switch()`); nothing bounces, but the core's debounce still makes a change wait 20 ticks.
- `log_transport()` is in the event record. A run that never stops or changes clock logs none, so existing counts of events hold.
- Time is scripted too: `timebase_elapsed_ticks()` returns whatever the test last set, on every call, so a test moves time on by calling `app_run_once()` once per tick.
- No hardware access, no register headers.
- This code is in the `misra-host` analysis run, so it is held to the coding standard like firmware source.
- Its coverage is reported by `make coverage` but not enforced.
- Control functions for tests (scripting inputs, reading the record) are declared in `host_ports.h`, never in a port header.
