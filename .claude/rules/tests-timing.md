---
paths:
  - "tests/test_seq*.c"
  - "tests/test_app*.c"
  - "tests/seq_fixture.h"
  - "tests/app_fixture.h"
---

# tests: timing in the sequencer's and the app loop's tests

Loaded with the files below, on top of `tests/CLAUDE.md`, whose rules apply to all of `tests/`.

- The first step is a whole step.
  A pattern started on call 1 begins step n on call 1 + n × (ticks per step); in `test_app`, each is applied and logged one tick later.
- A press of the run button is 20 ticks of the button held, and it counts on the twentieth.
  The debounce takes its first reading as it stands and the transport starts as if the button were held, so run at least one tick with the button up before a press, or it is not one.
- A pattern starts on the internal clock, and an edge takes it over.
  For one that waits on the external clock from its first tick, the sequencer's tests set the transport's latch in the state straight after `seq_init()` (`wait_for_external()` in `seq_fixture.h`), which is what an edge while stopped leaves; the app's tests have to get there with a stop, an edge and a start.
- An external edge is one tick with the edge flag set.
  Clear the flag again before the next tick, as the port does.
- In `test_app`, giving a selector position an amount takes the pot down to 0 and up again with a scan after each (`set_amount()`, 20 ticks).
  The tempo is not disturbed by it, since the pot is not on the tempo's position.
  Allow a scan (8 ticks) for any change of the panel, the pot or the LFO to be seen.
- The random numbers begin 135, 35, 70, 220, 176, 221, 238, 248 from the sequencer's seed.
  The plain engine draws none, so with it a step chance from the LFO takes them in that order.
- In `test_app`, MIDI bytes are written one tick after the core works them out, like every output, and clocks keep coming while the pattern is stopped.
  Find a message by searching for it, or compare with the clocks filtered out (`expect_midi_without_clocks()`), never by a remembered index.
