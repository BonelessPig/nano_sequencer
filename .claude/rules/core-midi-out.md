---
paths:
  - "core/midi_out.*"
---

# core: MIDI out

Loaded with the files below, on top of `core/CLAUDE.md`, whose rules apply to all of `core/`.

- An event (a start, a stop, a clock, a note starting or ending) is marked as owed in the state and sent when there is room, in a fixed order within a tick: Stop, Start, clocks, Note On, Note Off.
  Nothing is produced that does not fit, and only whole messages.
- `midi_out_tick()` is two halves: mark what the tick adds to what is owed, then send what fits.
  Sending is a `fits()` check and then a put that does not check.
  A put that checked and reported would sit on the right of an `&&` (a rule 13.5 finding) or leave a "did not fit" branch that cannot be reached.
- A one-byte message that cannot go means there is no room at all, so nothing after it can go either.
  Do not guard the later messages against it: the guard would be a branch no test can take.
  Say so in a comment where the order depends on it.
- The module trusts the caller to give the gate as low on every stopped tick, and does not look at `b_running` to end a note.
- "No note" is 255, which is not a note number.
- A start places the downbeat by clock: at once on the internal clock, on the first pulse on the external one, because `seq` leaves an external start one pulse short of due.
  Clocks owed from before a start are given up at it, and that is the only thing dropped.
- The channel, the velocities and the note of the lowest pitch are constants at the top of `midi_out.c`; the header gives them in words and the tests define their own.
