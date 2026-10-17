---
paths:
  - "tools/sim/scenarios/midi.test.js"
  - "tools/sim/scenarios/midi_clock.test.js"
  - "tools/sim/scenarios/midi_in.test.js"
  - "tools/sim/parts/midi_source.js"
  - "tools/sim/scenarios/midi_fixture.js"
  - "tools/sim/boards/midi_messages.js"
---

# tools/sim: MIDI out and in

Loaded with the files below, on top of `tools/sim/CLAUDE.md`, whose rules apply to all of `tools/sim/`.

- There is one wire from the USART and the board knows from its config what is on it: text for the debug image, which it sorts into lines, and MIDI for the midi image, which it keeps as bytes (`midi.bytes`) and reads into messages as they come (`midi.messages`).
  `serial.bytes` counts every byte whatever it was.
- A MIDI scenario about the clock, Start or Stop goes in `midi_clock.test.js`, and any other in `midi.test.js`.
  What the two share (the figures, `midiBoard()`, the assertions on what was sent) is in `midi_fixture.js`.
  A byte's cycle is the one in which the firmware handed it to the USART, when its start bit begins, and a message's is that of its first byte.
  Time a message from the pin edge it belongs to (a note from its gate, a step's first clock from the clock output), as a range: it waits for the bytes ahead of it at 320 µs each.
- Count MIDI clocks between one clock output pulse and the next (six, the first of them the step's own), not over a whole run: where a run ends decides how many of the last step's have gone.
  On the external clock the first edge gets one step whatever the ratio, so a clock multiplied by four gives 1 + 4 × (edges − 1) steps once it has stopped and played out.
- Check a note number twice over, against the panel and against the voltage the DAC held as the gate rose.
  Play the note messages back as a receiver would (`soundingAtEnd()` in `midi_fixture.js`) to show none is left on.
- `midi_messages.js` does not accept running status and calls anything but Start, Stop, the clock and the two note messages `unknown`.
  A scenario that asserts none is unknown has shown that no byte was dropped from inside a message.
- MIDI in is `parts/midi_source.js`, on the board as `midiIn`: `sendMidi(bytes)` queues bytes at 320 µs each, `startMidiClockIn(ms)` sends a clock byte on a period, and `midiIn.sent` lists each byte with the cycle it arrived in.
  A byte is handed to the USART whole as its last bit would arrive; only the midi image takes it, and a scenario for it goes in `midi_in.test.js`.
- The firmware counts every sixth clock as an edge and keeps the count through a stop.
  A scenario that needs an edge sends six clocks, or a Start first, which makes the next clock one.
- A step begins 1.2 to 2.2 ms after its clock byte arrives: read on the next tick, applied on the one after.
  With a clock period that is a whole number of ticks the steps are then exactly six periods apart.
- Two things sent on the same cycle go in the order they were asked for, so a Start sent as a clock falls due may land either side of it.
  Run to a time between two clocks before sending one.
