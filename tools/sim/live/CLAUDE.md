# tools/sim/live/ rules

`make live`: `server.js` runs the emulated board at real speed and `page.html`, with its parts in `page/`, shows it in a browser.
Beside it: `predict.js` (the transport and step it works out), `midi_out.js`, `page_files.js`.
A viewer, not a check; the board is in `tools/sim/CLAUDE.md`.

## Where the page is

`page.html` is only the frame: the head, the small sections, and the order of the rest.
Read the part a change concerns, not all of them.

- Sections, in `page/`, put in by the server where the frame has `<!-- part: name.html -->`: `panel.html`, `inputs.html` (the input jacks), `gear.html` (the patched gear), `midi.html`, `watchdog.html`, `log.html`, `cal-help.html`.
  Board, Calibration mode and Signals stay in the frame.
- `page/page.css`: all the style.
- Scripts, in `page/`, loaded in this order as plain scripts sharing one scope (no modules, no build step): `tables.js` (the tables, the shared state, `el()` and `post()`), `panel.js`, `outputs.js` (watchdog, crash log, outputs, MIDI out, `applyEvent()`), `trace.js` (the one chart of every signal, `drawTrace()`, with the board's bands of it, and `openCanvas()` for any canvas), `gear.js` (the patched gear and its bands of that chart, `drawGearBands()`), `wavetable.js` (the picture of its wavetable), `pots.js` (the rotary switches, and every `<input type="range">` drawn as a pot, the input kept inside it and still holding the value), `main.js` (the connection and every control's handler).
  Only `main.js` runs anything as it loads; the others just define, so a function may call one from a later file.
- The server serves `/`, and any `name.css` or `name.js` that is in `page/`; nothing else on disk.
  A new script needs its `<script>` in the frame, a new section its `part:` comment.
- Split a file by topic before about 20,000 characters.

## Rules

- The server uses `createBoard()` as the scenarios do and adds nothing to the model.
  Something new to show is exposed by the board first, then sent by the server.
- The page holds no emulator and no board state of its own: a control is posted to the server and drawn when the `state` event comes back.
- The patched gear (a VCO, an envelope and a VCA, shown on the chart under the board's signals) is the exception, because it is not the board: it is Web Audio in the page, driven by the `cv` and `gate` events, and the server knows nothing of it.
  It only listens to outputs.
  Gear that drives an input (an LFO into the modulation input, say) is a part in `tools/sim/parts/` wired by the board, as the clock source is.
- The server empties the board's records after every slice (`drain()`).
  A record added to the board has to be added to that list, or a long run fills memory.
- It is paced by wall time, so nothing measured through it counts; a timing question goes in a scenario.
- No dependencies beyond Node's own modules; it listens on 127.0.0.1 only.
- To try the server out, start your own on another port (`node tools/sim/live/server.js 8171`) and post only to that.
  The owner may have one running on the default port, 8165, with a session in it; never post to that port or stop what is listening there.
  Check that your own server bound its port before sending it anything.
- The page draws the panel as one that could be built: the steps in two rows of eight, the engine on one coded rotary switch, mode switches 7 to 3 as toggles under a legend.
  A control drawn there is a part that exists, wired as the board has it; a part the firmware could not read as drawn goes in the roadmap's panel hardware options instead.
- What explains the page goes in a table inside a `<details>`, closed until opened, not in a paragraph.
- A watchdog reset is taken from the board's `resets`, never from the log, so the release image shows it too.
  What the server works out (calibration mode, the transport, the step) is then worked out afresh as at power-on; anything new it predicts is put back in `startAfresh()`.
- The envelope CV is shown as a figure under Signals, from the board's `envWrites`, as an `env` event when it changes.
  The patched gear's own envelope is made in the page from the gate, not from this.
- The level light goes out as `level` in every `run` message, the part of the slice it was on for, from `takeLevel()`; it is no event and not in `drain()`'s list.
  The selector, LFO reading and rate pot are kept in `panel` and set again in `powerUp()`.
- The profile button is held: the page posts `profile: true` then `false`, and `run` has `profileHeldMs` (null while up).
  The status light is `status` in `run`, as the slice left it; `powerUp()` and `startAgain()` put it out.
- "Stop the tick" is asked of the board again every slice until the reset comes: a slice can end inside the tick interrupt, where `stopTick()` does nothing.
  It can only crash at place 5, and the page says so.
- The crash log shown is `crashLog()`, looked at after every slice and sent when it has changed.
  It is not a record that grows, so it is not in `drain()`'s list; `resets` is.
- The images offered are the board's `CONFIGS`, and what an image has is asked of the board (`hasLog`, `sendsMidi`), not its name.
  An image with no log says so in the log pane, and its transport and step come from the controls.
- MIDI out is shown from the board's `midi.messages`: an event for each message but the clocks, which get one event a slice however many there were.
  What `midi_out.js` keeps of them goes out whole as `midi` in the state and `now` in every `midi` event, so the page keeps nothing.
  It is put back in `powerUp()` and left alone in `startAgain()` on purpose: the firmware's own Start begins the count again, and a note cut short by a watchdog reset is still sounding at the receiver.
- A MIDI Start or Stop is believed for the transport as a `Transport = N` line is.
  It must not start the step count again: on the internal clock it comes after the first step's clock pulse, which has been counted by then.
