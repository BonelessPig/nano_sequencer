# adapters/host/ rules

The ports faked for the PC: tests script what the input ports return and read back a record of every output-port call, in order.

- `host_ports.c` implements every function in `ports/`. A new port function needs its fake here in the same change, or `test_app` will not link.
- An output written on every tick (the gate, the clock output, the pitch CV, the step lights) is not put in the event record, which would fill in 64 ticks. The fake keeps its level, or the voltage last written successfully, and counts rising edges or writes; `host_event_count_at_gate_rise()` is what shows whether the gate or the log came first, and `host_cv_millivolts_at_gate_rise()` whether the pitch was in place before the gate.
- The non-volatile storage is a small array that `host_reset()` erases and `app_init()` does not, so a test powers the app off and on by calling `app_init()` again. A write lands at once; there is no write time to poll out. `host_set_nv_bytes()` puts a record there beforehand.
- `log_startup()` is counted and its arguments kept, but it is not put in the event record, so that the record of every run still begins with its first step. `log_crash()` is treated the same way.
- The breadcrumb is a byte that `host_reset()` clears and `app_init()` does not, like the storage, so a test resets the app by setting a reset cause and calling `app_init()` again; `host_set_breadcrumb()` puts a place there beforehand. The fake keeps the last eight places written, newest first, and what the byte held at the last pitch CV write and the last run button read, which is how a test sees that a part of the tick was marked before it began.
- The storage is 64 bytes: the calibration record from 0 and the crash log from 16.
- The clock and reset inputs are each a pending edge: `host_pulse_clock_input()` sets it, and the next read takes it and forgets it, as the hardware's flag does. The run button and the sync switch are levels (`host_set_run_button()`, `host_set_sync_switch()`); nothing bounces, but the core's debounce still makes a change wait 20 ticks.
- The MIDI output gives the room a test sets (`host_set_midi_room()`) and records every byte it is written, at write time, a write of more than the room included, so that a test can see an overrun. The bytes are kept out of the event record and nothing is sent on a poll. Two readbacks show the order of the apply step: `host_midi_byte_count_at_gate_rise()` and `host_midi_byte_count_at_first_event()`.
- A static here is named for its fake (`g_host_midi_*`): a name shared with a static in another file of the host run is a rule 5.9 finding.
- `log_transport()` is in the event record. A run that never stops or changes clock logs none, so existing counts of events hold.
- The scripted panel is nine bytes, the ninth being the mode switches. A test's array of eight leaves them all off, which is the plain engine.
- Time is scripted too: `timebase_elapsed_ticks()` returns whatever the test last set, on every call, so a test moves time on by calling `app_run_once()` once per tick.
- No hardware access, no register headers.
- This code is in the `misra-host` analysis run, so it is held to the coding standard like firmware source.
- Its coverage is reported by `make coverage` but not enforced.
- Control functions for tests (scripting inputs, reading the record) are declared in `host_ports.h`, never in a port header.
