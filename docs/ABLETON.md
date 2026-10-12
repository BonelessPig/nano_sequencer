# The sequencer in Ableton Live: options

Nothing here is decided or built.
This is the comparison of four ways to run the sequencer inside Ableton Live, written to choose from.
When a route is chosen it becomes a decision in [DECISIONS.md](DECISIONS.md) and a phase in [ROADMAP.md](ROADMAP.md).

A claim marked **verified** was read from a primary source or run on this machine in October 2026.
One marked **unverified** is from memory or a secondary source; check it before relying on it.
Every effort figure is a guess, in working days of one person, and none has been tested against a build.

## 1. What could be ported

Three separate things exist today, and each route picks from them.

| Thing | Where it is | What it is |
|---|---|---|
| The emulated chip | `tools/sim/lib/`, `parts/`, `boards/`, avr8js 0.21.1 | JavaScript. Runs `build/<config>/output.hex` unmodified on a model of the ATmega328P with the board's parts round it. |
| The native core | `core/`, `app/app.c`, `ports/` | C. The sequencer's logic, which already runs on a PC against `adapters/host/host_ports.c`. |
| The voice | `tools/sim/live/page/gear.js`, `wavetable.js` | JavaScript on Web Audio. A VCO with wavetables and a chorus, an envelope and a VCA. It listens to the pitch CV and the gate and is no part of the board. |

Four facts shape everything below.

- **The emulator is not cheap.**
  The firmware never sleeps, so all 16 million cycles a second have to be run.
  Measured here under Node 22, the board runs at 2.6 times real speed, which is 38 % of one CPU core at real speed, the same for all three images (**verified**, `createBoard()` run for three seconds of chip time).
  A JavaScript engine with no JIT would be tens of times slower and could not keep up (**unverified**).
- **The native core is nearly free to host.**
  Its whole hardware surface is the 21 headers in `ports/`, 33 functions, and one tick is one call of `app_run_once()` with the timebase reporting 1.
- **The native core holds one sequencer per process.**
  `app/app.c` keeps its state in file-scope statics and a port function takes no context.
  Linked into a plugin, two instances in one Live set would be the same sequencer; section 5 has the ways round it.
- **The voice needs neither of the others.**
  It is a monophonic synth that follows a pitch and a gate, so it can be a separate device that takes MIDI notes.

The two sequencers differ in what they prove.

| | Emulated chip | Native core |
|---|---|---|
| Runs | The image as flashed: start-up, registers, interrupts, every target adapter | `core/` and `app/` only, compiled for the PC |
| Watchdog, crash log, EEPROM, calibration mode | All there | Faked in the new adapter, or left out |
| Timing against Live | The chip's own clock, to be kept in step with Live's | Counted in Live's samples |
| Cost | 38 % of a core each | Negligible |

## 2. The four routes

### A. Max for Live, the emulated chip in `node.script`

**What it is.**
A Max for Live MIDI effect.
`node.script` runs a Node process of its own (**verified**, Cycling '74 reference), and in it the emulated board as `make live` runs it.
The existing page is shown in a `jweb` object from the server on 127.0.0.1.
MIDI bytes from the board go to Max and out of the device.

```
Live transport --> [plugsync~] --> node.script --> board clock and reset jacks
                                   (avr8js + .hex)
                                        |  board.midi, stamped with chip time
                                        v
                               [delay to a fixed lateness] --> [midiout] --> next device
                   jweb <--- http://127.0.0.1:port (the existing page)
```

**Reused.**
All of `tools/sim/lib/`, `parts/` and `boards/`, `live/server.js`, and `live/page/` as it stands.
The three `.hex` images.

**New.**

- `tools/sim/live/max/entry.js`, about 150 lines: starts the server on a free port, sends each MIDI message through `max-api` with its chip time, and turns Live's play, stop and clock into `pressRunButton()`, `pulseResetIn()` and clock pulses.
- `server.js` split so that its board and slice loop can be reached from that entry; it is at 38,000 characters, over the folder's limit, so it is due a split anyway.
- One `.amxd` patch: `node.script`, `jweb`, `plugsync~` or `transport`, a delay and `midiout`.

**Build and toolchain.**
None added to the repository.
Needs Live Suite, or Live Standard with the Max for Live add-on (**unverified**).
The device needs Node, `node_modules/avr8js` and the `.hex` beside it; Max ships a Node of its own, of a version the documentation does not give (**verified** that it is not stated; find it with `process.version` in the device).

**Stages.**

| Stage | Work | Days |
|---|---|---|
| A1 | Split `server.js`; entry script; notes heard in Live from the midi image, started by hand | 2 |
| A2 | Stamp events with chip time and play them a fixed time late; measure the spread | 1 |
| A3 | Follow Live's transport and tempo | 2 |
| A4 | The page in `jweb`, the panel saved with the Live set | 2 |

**Unknowns.**

- How late and how uneven a message from `node.script` is.
  The documentation gives no figure (**verified** that it is silent).
  A2 measures it: record the device's MIDI into a clip and look at the note starts against the grid.
- Whether `jweb` loads a page with server-sent events and Web Audio inside Live.
  One forum thread reports `window.max` missing in `jweb` under Max for Live (**unverified**); this route does not need it, because the page talks to the server over HTTP.
- Whether 38 % of a core beside Live's audio thread is tolerable on the owner's PC.

**When the firmware changes.**
Rebuild, and copy the three `.hex` files into the device.
A new control needs what `make live` already needs and nothing more.

### B. Max for Live, the native core as a C external

**What it is.**
A Max external written in C that links `core/`, `app/app.c` and a new adapter, inside a Max for Live MIDI effect.
A Max clock calls `app_run_once()` every millisecond.

```
Live transport --> [plugsync~] --> [nanoseq] external --> [midiout] --> next device
                                   core/ + app/ + adapters/max/
                   live.dial, live.toggle ... --> inlets (the panel, rebuilt in Max)
```

**Reused.**
`core/`, `app/app.c` and `ports/` unchanged.
`adapters/host/host_ports.c` as the pattern for the adapter.

**New.**

- `adapters/max/max_ports.c`, about 400 lines: the 33 port functions, the panel held in a struct the inlets write, MIDI bytes handed to an outlet.
- `daw/max/nanoseq.c`, about 300 lines: the Max object, its inlets and outlets, the clock.
- The `.amxd` patch with the panel drawn in Max objects: sixteen note dials, the mode switches, the selector and its pot.

**Build and toolchain.**
The Max SDK, CMake and Visual Studio on Windows; a second build on macOS for anyone with a Mac (**unverified**, from memory of the SDK).
The external is frozen into the device.

**Stages.**

| Stage | Work | Days |
|---|---|---|
| B1 | SDK set up; an external that builds and loads | 2 |
| B2 | The adapter and the object; notes out with a fixed panel | 3 |
| B3 | The panel in Max, parameters automatable and saved | 3 |
| B4 | Live's transport; a second instance refused (section 5) | 2 |

**Unknowns.**

- How exact Max's millisecond clock is inside Live.
  From memory Max for Live runs its scheduler in the audio callback, which would put a tick within one audio buffer of where it belongs (**unverified**).
  A signal-rate object (`nanoseq~`) counting samples would be exact, at the cost of a little more code.
- Whether the existing page could be the panel.
  It would need `jweb` and `window.max`, which the forum thread above puts in doubt.

**When the firmware changes.**
Rebuild the external.
A new port function needs its line in `max_ports.c`, as it does in `host_ports.c`; a new control needs a Max object in the patch.

### C. A JUCE plugin (VST3), the native core

**What it is.**
A VST3 plugin in C++ whose audio callback runs the core.
For each block it calls `app_run_once()` once for every millisecond of samples and places what the core put out at the exact sample.

```
Live playhead (tempo, position) --> processBlock() --> MIDI out, at the sample
                                    core/ + app/      --> pitch, gate, envelope, clock as audio (CV)
                                    + adapters/plugin/ --> the voice, as audio (optional)
                   WebView editor <--> the plugin (live/page/, re-pointed)
```

**Reused.**
`core/`, `app/app.c` and `ports/` unchanged.
`live/page/` as the editor: JUCE 8 can show a web page as a plugin's window and pass messages both ways (**unverified**, from memory), so `post()` in `tables.js` and the event stream in `main.js` are the two places to re-point.
The page's layout of the panel, the outputs and the MIDI pane.

**New.**

- `adapters/plugin/plugin_ports.c`, about 400 lines, as in B.
- `daw/juce/`: the processor (about 500 lines), the parameters, the editor's bridge (about 300), and a `CMakeLists.txt`.
- What `server.js` works out for the page (the transport, the step, the notes sounding) done again in C++ or dropped, since the native core can simply be asked.
- If the voice is wanted: `gear.js` and `wavetable.js` again in C++, about 600 lines.

**Build and toolchain.**
CMake, Visual Studio and JUCE 8.
JUCE is free for up to $20,000 a year of revenue, with no splash screen (**verified**, JUCE forum announcement), and otherwise AGPLv3 or paid (**unverified**).
This is by far the most machinery the project would have, so it belongs in `daw/` or a repository of its own, outside `make check`.

**Stages.**

| Stage | Work | Days |
|---|---|---|
| C1 | JUCE set up; an empty plugin that loads in Live | 2 |
| C2 | The adapter; notes out at the sample, fixed panel | 3 |
| C3 | Live's playhead as the clock and reset; parameters | 3 |
| C4 | The page as the editor | 5 |
| C5 | CV outputs as audio | 2 |
| C6 | The voice | 5 |

**Unknowns.**

- Where it sits in Live.
  Live has no MIDI effect slot for a VST: the plugin goes on a track of its own, and a second track takes its MIDI with "MIDI From" set to that track and the plugin chosen beneath (**verified**, Ableton help article).
  An Audio Unit cannot send MIDI out at all (**verified**, same article), so on a Mac it is VST3 only.
- How much of the page survives without the server behind it.
- CV as audio needs a DC-coupled audio interface.

**When the firmware changes.**
Rebuild the plugin.
A new port function needs its line in the adapter, and a new control a parameter and whatever the page needs.

### D. A JUCE plugin (VST3), the emulated chip

**What it is.**
The plugin of route C with an AVR emulator in place of the native core, so that the real image runs and its outputs are still placed at the sample.
avr8js cannot do this: it is JavaScript, and nothing with a JIT can run on an audio thread.
There are three ways to get an emulator there.

| Way | What it means | Trouble |
|---|---|---|
| D1: simavr | A C emulator of the same chip, run for 16,000 cycles a millisecond in the audio callback | GPLv3 (**verified**, Debian and Fedora packaging), so the plugin is too. Made for Linux and macOS; no Windows build was found (**verified** that none turned up). Its speed here is unknown. |
| D2: an emulator of our own | The AVR core and the seven peripherals the firmware uses, in C++ | About 2,500 lines to write and then to prove against avr8js, scenario by scenario. |
| D3: avr8js in the editor's WebView | The existing JavaScript, in the plugin's window | The chip stops when the window is closed, and timing is no better than route A. Not worth building. |

```
Live playhead --> processBlock() --> emulator (.hex) --> parts in C++ --> MIDI, CV at the sample
                                     16,000 cycles per millisecond
```

**Reused.**
The three `.hex` images, and `live/page/` as in C.
`tools/sim/parts/` and `boards/nano_sequencer.js` only as the specification for a rewrite.

**New.**
Everything new in C but the adapter, and in its place: the emulator, the 74HC165, 74HC595 and MCP4822 models, the edge flags and the board's wiring, all again in C++ (about 1,200 lines beside the emulator).
From then on there are two models of the board to keep the same.

**Build and toolchain.**
As C, plus simavr's own build for D1.

**Stages.**

| Stage | Work | Days |
|---|---|---|
| D1.1 | simavr built on Windows and running the image headless; speed measured | 3 to 6 |
| D1.2 | The parts and the board in C++, checked against `make sim`'s scenarios | 6 |
| D1.3 | Into the plugin of C1 to C4 | 13, shared with C |

**Unknowns.**

- Whether an emulator can be relied on to finish its 16,000 cycles inside every audio buffer.
  A late buffer is a click, where in route A a slow slice is only a late note.
- simavr on Windows, and its speed.
- Whether simavr's peripherals behave as avr8js's do for this firmware; `tools/sim/CLAUDE.md` lists what had to be worked round in avr8js, and simavr will have a list of its own.

**When the firmware changes.**
Rebuild and copy the `.hex`.
A new part on the board is modelled twice, in JavaScript and in C++.

## 3. Comparison

### Timing and cost

| | A | B | C | D |
|---|---|---|---|---|
| Where a note lands | Uneven by milliseconds; even if played a fixed 20 to 30 ms late | Within one audio buffer; exact as `nanoseq~` | At the sample | At the sample |
| Following Live's tempo | Pulses reach the chip up to a slice late (10 ms now, could be 2) | Exact | Exact | Exact |
| CPU for one | 38 % of a core, in a process of its own | Negligible | Negligible | Unknown; all of it on the audio thread |
| Several in one set | Yes, a process each | One, until section 5 is done | One, until section 5 is done | Yes |
| What a slow moment costs | A late note | Nothing likely | Nothing likely | A click |

### What really runs

| | A | B | C | D |
|---|---|---|---|---|
| `core/` and `app/` | Yes, as AVR code | Yes, as PC code | Yes, as PC code | Yes, as AVR code |
| `adapters/target/`, start-up, interrupts | Yes | No | No | Yes |
| The three images, chosen in the device | Yes | No; MIDI out is simply on | No | Yes |
| Watchdog and crash log | Yes | No | No | Yes |
| EEPROM and calibration mode | Yes | Only if the adapter fakes storage | Only if the adapter fakes storage | Yes |
| The debug log | Yes | Could be shown; it is plain calls | Could be shown | Yes |
| Says something about the firmware | Yes, what `make live` says | No more than `make test` does | No more than `make test` does | Yes, on a second emulator |

### What it can put out

| | A | B | C | D |
|---|---|---|---|---|
| MIDI notes and clock | Yes | Yes | Yes | Yes |
| CV as audio | Poorly: a MIDI effect has no audio out, and the values arrive unevenly. Live's CV Tools turn its MIDI into CV with no work. | As `nanoseq~` in an audio device | Yes | Yes |
| Its own voice | Not in this device; a second device after it (section 6) | A Max voice in the same device or after it | Yes, in the plugin | Yes, in the plugin |

### In Live

| | A | B | C | D |
|---|---|---|---|---|
| Sits | In the track, ahead of the instrument | In the track, ahead of the instrument | On a track of its own; another track takes its MIDI | As C |
| Automation and Push | Only what is given a Max parameter; the page in `jweb` is not | Every `live.*` object, for free | Every plugin parameter | As C |
| Saved with the set | The panel, if stored in the patch; the EEPROM, if written to a file | Parameters, for free | Plugin state | As C, plus the EEPROM |
| Other hosts | No | No | Any that load VST3 | As C |
| Needs | Max for Live | Max for Live | Any edition of Live that loads VST3 | As C |

### Work

| | A | B | C | D (by simavr) |
|---|---|---|---|---|
| Days to first notes | 2 | 5 | 5 | 9 to 12 |
| Days to finished, MIDI only | 7 | 10 | 13 | 22 to 25 |
| With CV and the voice | Voice apart: 3 to 5 more | Voice in Max: 3 to 5 more | 20 | 29 to 32 |
| Reused as it is | The whole emulated board, the server, the page | The core and app | The core and app, most of the page | The images, most of the page |
| New code, lines | 150 and a patch | 700 and a patch | 1,200, or 1,800 with the voice | 2,400 and simavr, more with the voice |
| Toolchain added | None | Max SDK, CMake, Visual Studio | JUCE, CMake, Visual Studio | As C, and simavr |
| Licence brought in | None | Max SDK's | JUCE's: free tier or AGPLv3 | JUCE's and GPLv3 |
| Shared with someone else as | A folder: device, Node modules, images | One frozen `.amxd`, per platform | One `.vst3`, per platform | One `.vst3`, per platform |
| Risk | Low; the unknown is timing, found in three days | Medium; the SDK is new ground | Medium; a lot of it | High; three unknowns before the first note |

### Against the project's rules

| Rule | A | B | C | D |
|---|---|---|---|---|
| Minimal and lightweight | Kept | A second build system | The heaviest thing in the project | Heavier still |
| GNU Make only | Kept | Broken, in `daw/` | Broken, in `daw/` | Broken, in `daw/` |
| The live view has no dependencies beyond Node's own | `max-api`, which Max supplies and only the entry script uses | Not touched | Not touched | Not touched |
| The core and app unchanged | Yes | Yes, unless section 5's third way | Yes, unless section 5's third way | Yes |
| The coding standard and MISRA | Tooling, outside it | A new adapter in C: inside it, if kept in `adapters/` | The same; the C++ is outside it | C++ only, outside it |
| One model of the board | Kept | Not concerned | Not concerned | Broken: two to keep the same |

## 4. What each route is for

- **A** shows the firmware itself playing in a Live set, soonest and with least written.
  It is a viewer that makes sound, with a viewer's timing.
- **B** is the sequencer as a native Live device: in the track, automatable, on Push.
  It runs the logic and not the firmware.
- **C** is the sequencer as an instrument for any host, with everything it can put out.
  It is the most work that is still straightforward.
- **D** is the only one that is both the real image and exact, and it pays for that three ways: an emulator on the audio thread, a second model of the board, and a licence.
  What it would prove, that the image behaves, `make sim` already proves with the timing counted in cycles.

## 5. One sequencer per process (routes B and C)

`app/app.c` lines 84 to 105 are file-scope statics, and so is the state of any adapter.
Every instance of a plugin in a Live set is in the same process.

| Way | What it takes | Cost |
|---|---|---|
| One instance | The second to load says so and does nothing | An hour. A real limit: no two sequencers in a set. |
| `wasm2c` | `core/` and `app/` compiled to WebAssembly and turned back into C, which keeps each instance's memory in a struct of its own; the ports become imports | No firmware source changes. Two more tools in the build (**unverified** that it does what is said here; it is from memory). |
| State in a struct | `app.c`'s statics moved into a struct passed to `app_init()` and `app_run_once()`, and a context given to the ports | Changes the firmware for the plugin's sake, in every port and every test. Not recommended. |

Start with one instance, and take `wasm2c` only if two are ever wanted.

## 6. Two plugins

The owner's idea: one device that is just the hex sequencer, and one that has its own voice.
It works well, because the voice needs nothing from the sequencer but notes.
The pairings, cheapest first:

| Pairing | Sequencer | Voice | Days | Shared | Written twice |
|---|---|---|---|---|---|
| 1 | A | A Max for Live instrument: the wavetables and envelope in gen~ or MSP | 10 to 12 | The page shows both: the voice's pots could stay Max's | The voice, once more beside `gear.js` |
| 2 | A | A small JUCE synth, voice only | 14 to 16 | Nothing but MIDI | The voice, and JUCE arrives for it alone |
| 3 | A, and C beside it | Inside C | 27 | The page, the images' behaviour | Nothing new, but two sequencers to keep |
| 4 | B | A Max for Live instrument | 13 to 15 | One Max toolchain | The voice |

In pairings 1 and 4 both devices sit on one track, sequencer first, and the voice plays whatever MIDI reaches it, so it also works as an instrument on its own, from a clip or a keyboard.
Pairing 3 is the owner's two instances as first put: a plugin with the native core and the voice, and a device with the hex.
It is the dearest, and it is also where the work would end up if pairing 1 were built first and C later, with nothing thrown away but the Max voice.

The voice as it is has a limit worth knowing before it is ported: it follows the pitch CV and the gate that the page is sent every 10 ms, so its own timing is the live view's.
As a device taking MIDI it gets Live's timing instead.
The modulation's envelope on the DAC's channel B has no MIDI message today, so a voice fed by MIDI uses an envelope of its own, as `gear.js` does now.

## 7. Smaller alternatives

- **`make live` and a virtual MIDI port.**
  The server sends the board's MIDI to a loopback port (loopMIDI on Windows) and Live listens.
  A day's work and the same timing as A, but Node has no MIDI of its own, so it needs a native module, which the live view's rules forbid.
  A does the same from inside Live.
- **iPlug2 or DPF in place of JUCE.**
  Both are smaller and under permissive licences, and iPlug2 can show a web page as the editor (**unverified**).
  Worth a look if C is chosen and JUCE's size or licence is the objection.
- **CLAP.**
  A plugin format in plain C, which would suit this project better than any other, but Live does not load it (**unverified**).
- **gen~ or RNBO.**
  Neither takes C.
  Either would be the core written again in another language, with two to keep in step.
- **The board itself.**
  The midi image already sends MIDI from the Nano; a MIDI interface puts it into Live with nothing written.
  It is unrun on hardware, like everything else.

## 8. Recommendation

1. **Build A first, to stage A2: three days.**
   It is what was asked for, it reuses everything, and it ends with the one number that decides the rest: how unevenly the notes land, measured from a recorded clip.
2. **If that number is good enough, finish A and add the voice as a Max for Live instrument** (pairing 1).
   The hex sequencer and a voice, in one track, with no new toolchain.
3. **If it has to be exact, go to C**, one instance, MIDI first, and keep A as the device that shows the firmware.
   Take B instead only if it will never need to leave Live or put out audio.
4. **Leave D.**

The first step of each, if another is chosen:

| Route | First step | Settles |
|---|---|---|
| A | Print `process.version` from a `node.script` in Live, then run the board there and read the speed figure | Whether the bundled Node runs avr8js at real speed beside Live |
| B | Build the SDK's example external and load it in a Max for Live device | Whether the toolchain is tolerable |
| C | Build JUCE's example plugin with a web editor and load it in Live, with a second track taking its MIDI | Whether Live's routing is acceptable to work with |
| D | Build simavr on Windows and time it on `build/midi/output.hex` | Whether D is possible at all |

## 9. Sources

- Cycling '74, `node.script` reference: <https://docs.cycling74.com/reference/node.script>
- Cycling '74, Web Browser and jweb: <https://docs.cycling74.com/userguide/web_browser/>
- Cycling '74 forum, `window.max` not working in jweb with Max for Live: <https://cycling74.com/forums/window-max-not-working-in-jweb-with-max4live>
- Ableton, Accessing the MIDI output of a VST plug-in: <https://help.ableton.com/hc/en-us/articles/209070189-Accessing-the-MIDI-output-of-a-VST-plug-in>
- JUCE forum, Amendments to the JUCE End User Licence Agreement for JUCE 8: <https://forum.juce.com/t/amendments-to-the-juce-end-user-licence-agreement-for-juce-8/61265>
- JUCE forum, Revenue limits for JUCE tiers: <https://forum.juce.com/t/revenue-limits-for-juce-tiers/61058>
- Debian, simavr copyright file: <https://metadata.ftp-master.debian.org/changelogs/main/s/simavr/stable_copyright>
