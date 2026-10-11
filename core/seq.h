#ifndef SEQ_H
#define SEQ_H
/**
 * @file   seq.h
 * @brief  Sequencer core: pure logic with no hardware access. It turns one
 *         snapshot of the inputs into one set of outputs, and compiles
 *         unchanged for the MCU and for a PC.
 * @author BonelessPig
 *
 * @copyright Copyright (c) 2026
 *
 */
#include <stdbool.h>
#include <stdint.h>
#include "address.h"
#include "clock.h"
#include "engine_cartesian.h"
#include "engine_chance.h"
#include "engine_euclid.h"
#include "engine_klee.h"
#include "engine_plain.h"
#include "engine_stage.h"
#include "gate.h"
#include "mod.h"
#include "note_map.h"
#include "panel.h"
#include "pitch_cal.h"
#include "prng.h"
#include "transport.h"

// A step is a sixteenth note: four to the quarter note, 24 clock pulses each
#define SEQ_STEPS_PER_BEAT   (4U)
#define SEQ_PULSES_PER_STEP  (CLOCK_PPQN / SEQ_STEPS_PER_BEAT)

// The clock output is high for the first half of every step
#define SEQ_CLOCK_OUT_PULSES (SEQ_PULSES_PER_STEP / 2U)

// Tempo: the control's 10-bit range maps linearly onto 30 to 285 beats per
// minute, so there is a floor at the slow end and no way to stop the clock.
#define SEQ_BPM_MIN           (30U) // Tempo with the control at 0
#define SEQ_TEMPO_RAW_PER_BPM (4U)  // Tempo reading units per beat per minute

/**
 * @brief State the sequencer carries from one tick to the next.
 */
typedef struct
{
    clock_state_t        clock;          // Turns elapsed ticks into pulses, at the
                                         // tempo or following the external clock
    uint16_t             last_tempo_raw; // Most recent valid tempo reading, 0 to 1023
    uint16_t             step_pulses;    // Pulses since the last step was due; the
                                         // next is due at SEQ_PULSES_PER_STEP
    uint8_t              engine;         // Which engine played the last step: the
                                         // mode switches' choice, as seq_tick() says
    engine_plain_state_t plain;          // Where the plain engine has got to
    engine_stage_state_t stage;          // Where the stage engine has got to
    engine_klee_state_t  klee;           // The Klee engine's register
    engine_chance_state_t    chance;     // Where the chance engine has got to
    engine_cartesian_state_t cartesian;  // Where the Cartesian engine has got to
    engine_euclid_state_t    euclid;     // Where the Euclidean engine has got to
    prng_state_t         prng;           // The random numbers the engines draw from
    gate_state_t         gate;           // How much of the current note's gate is left
    uint16_t             cv;             // Pitch CV value of the last note played
    uint16_t             leds;           // Step lights of the step last due: what
                                         // is lit while the clock output is high
    transport_state_t    transport;      // Whether the pattern runs, and from which clock
} seq_state_t;

/**
 * @brief The modulation: a slow signal from outside (an LFO), and how much
 *        of it is let through to each thing it can move. Read when a step
 *        begins.
 */
typedef struct
{
    uint16_t lfo;         // The signal, 0 to 1023, with 512 its centre, where
                          // it moves nothing (see mod.h)
    bool     b_lfo_valid; // false if lfo could not be read; it is then taken
                          // to be at its centre
    uint16_t transpose;   // Amount for the note, 0 to 1023; 0 is off
    uint16_t gate_length; // Amount for the gate length, the same
    uint16_t chance;      // Amount for the chance of a step dropping out
} seq_mod_t;

/**
 * @brief One snapshot of the outside world, gathered before a tick.
 */
typedef struct
{
    // The panel's step bits and mode switches, packed as panel.h describes
    uint8_t           raw_steps[PANEL_RAW_BYTE_COUNT];
    bool              b_steps_valid; // false if raw_steps could not be read
    uint16_t          tempo_raw;     // Tempo control position, 0 to 1023 (10-bit)
    bool              b_tempo_valid; // false if tempo_raw could not be read
    uint8_t           elapsed_ticks; // Timebase ticks (milliseconds) since the previous tick
    address_config_t  address;       // Direction, first and last step, one-shot
    note_map_config_t note_map;      // Scale and root
    gate_config_t     gate;          // Gate length
    pitch_cal_config_t pitch_cal;    // Pitch CV value of each octave
    bool              b_reset;       // true to send the pattern back to its start
    bool              b_clock_edge;  // true if an edge arrived on the external clock
                                     // since the previous tick
    bool              b_run_button;  // true while the run button is held down, as read
    seq_mod_t         mod;           // The LFO and its three amounts
} seq_inputs_t;

/**
 * @brief What the sequencer wants done after a tick.
 */
typedef struct
{
    bool    b_step_started; // true on the tick a step begins; the fields below
                            // describe that step, and are 0 and false otherwise
    uint8_t step;           // Panel step that began, 0 to PANEL_STEP_COUNT - 1
    bool    b_note_valid;   // false if the step inputs were not valid on that
                            // tick, so what the step holds is not known
    bool    b_rest;         // true if the step is valid and plays nothing
    uint8_t semitone;       // Pitch of the step if it is valid and not a rest:
                            // semitones above the lowest pitch, 0 to 45
    bool    b_gate;         // Level of the gate after this tick: true while a
                            // note is held. Given on every tick
    bool    b_clock_out;    // Level of the clock output after this tick: true
                            // for the first half of every step. Given on
                            // every tick
    uint16_t cv;            // Pitch CV value after this tick, 0 to
                            // PITCH_CAL_CV_MAX: that of the last note played,
                            // held through rests. Given on every tick
    uint16_t leds;          // Step lights after this tick, bit n for step
                            // n: those of the step that last began, lit
                            // while the clock output is high. Given on
                            // every tick
    transport_mode_t transport; // Whether the pattern is stopped, or running
                            // from the internal clock or the external one.
                            // Given on every tick
    bool    b_transport_changed; // true on a tick whose transport is not
                            // the last one's
    uint8_t pulses;         // Clock pulses that fell on this tick, 96 to
                            // the quarter note: those of the pattern's
                            // clock while it runs, and of the internal
                            // clock while it is stopped with no external
                            // edge since the stop. Given on every tick
} seq_outputs_t;

/**
 * @brief  Puts the state into its power-on condition: running, so that the
 *         first tick starts the pattern, with no tempo reading yet, so the
 *         tempo is SEQ_BPM_MIN until one arrives. The pitch CV value is 0
 *         until the first note. The random numbers start from the same
 *         seed every time, and nothing but this starts them again.
 * @param  p_state  State to initialize. Ignored if null.
 */
void seq_init(seq_state_t *p_state);

/**
 * @brief  Runs one tick: moves the clock on by the elapsed time and, if that
 *         brings a step due, begins it and works out its pitch. Reads
 *         *p_state and *p_in, writes *p_state and *p_out, and has no other
 *         effect.
 *
 *         The run button decides whether the pattern runs, and the edges of
 *         the external clock which clock it runs from (see transport.h).
 *         The button is debounced here and is given as read. The pattern is
 *         running after seq_init(), on the internal clock, and each press
 *         of the button stops it or starts it again. An edge takes it to
 *         the external clock, where it stays until it is stopped.
 *
 *         A start is the first tick after seq_init() and each tick on which
 *         the button starts the pattern again. It goes back to the first
 *         step of the pattern, and the time elapsed on that tick is not
 *         counted, so the step it begins is a whole one. On the internal
 *         clock that step begins on the same tick. On the external clock it
 *         begins on the first edge.
 *
 *         While the pattern is stopped nothing is due and nothing begins.
 *         The gate is closed at once and the clock output is low; the pitch
 *         CV value holds. Reset is ignored, and an edge of the external
 *         clock moves nothing: it only makes that the clock the next start
 *         is on. Only the count of clock pulses goes on, as said below.
 *
 *         On the internal clock a step lasts SEQ_PULSES_PER_STEP clock
 *         pulses, which is 15000 / BPM milliseconds.
 *
 *         On the external clock the pulses come from clock_follow() (see
 *         clock.h), and the tempo reading sets a ratio in place of a tempo:
 *         seven equal parts of its range give one step to every 4, 3 or 2
 *         edges, one step an edge in the middle, and 2, 3 or 4 steps an
 *         edge. With no edges, no step begins. If an edge comes so early
 *         that the clock output is still high from the step before, its
 *         step begins one tick late, and for that tick the gate is closed
 *         and the clock output low, so that every step has a rising edge
 *         of its own on both. Going to the external clock does not restart
 *         the pattern: the step in progress ends there and the next begins
 *         on the edge that took the clock over. The way back to the
 *         internal clock is a stop and a start, which does.
 *
 *         Which step begins is up to the engine and the addressing
 *         controls in p_in->address (see address.h). The low three mode
 *         switches (see panel.h) choose the engine, as a number: 1 is the
 *         stage engine (engine_stage.h), 2 the Klee engine (engine_klee.h),
 *         3 the chance engine (engine_chance.h), 4 the Cartesian engine
 *         (engine_cartesian.h), 5 the Euclidean engine (engine_euclid.h),
 *         and 0 and every other value the plain engine (engine_plain.h).
 *         The other five mode switches are the chosen engine's to give a
 *         meaning to. The choice is read when a step
 *         begins, from the panel as last read, and a change of engine
 *         starts the new one's pattern from its beginning. When a one-shot
 *         pattern has finished, the clock keeps running and steps keep
 *         falling due, but none begins.
 *
 *         While p_in->b_reset is true the pattern is held at its start: the
 *         next step to begin is the first of the cycle, at the time it
 *         would have begun anyway. A reset does not move the clock.
 *
 *         The step's note value is taken from the inputs of the tick on
 *         which it begins, so a change on the panel is heard the next time
 *         its step comes round. A value of 0 is a rest; any other is turned
 *         into a pitch by the scale and root in p_in->note_map (see
 *         note_map.h). A step begins on time even if the step inputs are
 *         not valid (it is then neither a rest nor a pitch), so a failed
 *         read does not shift the pattern in time. At most one step begins
 *         per tick: after a stall of more than a step, the steps owed begin
 *         on successive ticks.
 *
 *         A step that plays a pitch opens the gate for p_in->gate.length
 *         clock pulses, read when the step begins (see gate.h). A rest, a
 *         step whose note is not valid, and a step that is due but does not
 *         begin all leave the gate closed, and each ends a gate still open
 *         from the step before. A reset does not cut a held note short.
 *         A note the engine ties to the next step is held for a whole step
 *         at least, whatever the gate length, so its gate is still open
 *         when that step begins.
 *
 *         A step that plays a pitch also sets the pitch CV value, through
 *         the calibration table in p_in->pitch_cal (see pitch_cal.h), on
 *         the tick its gate opens. Nothing else changes it: it holds
 *         through rests, unreadable steps, a finished pattern and a reset.
 *
 *         The clock output follows the clock and not the pattern: it rises
 *         each time a step falls due, whether or not one begins, and falls
 *         SEQ_CLOCK_OUT_PULSES later. While steps are owed after a stall it
 *         stays low.
 *
 *         The step lights show the step that last began, for as long as
 *         the clock output is high, so a light flashes for the first half
 *         of its step and a step played twice flashes twice. It is the
 *         light of the step the engine gave, but for the Klee engine, where
 *         it is every active stage of the register (see engine_klee.h).
 *         Nothing is lit for a step that fell due and did not begin, nor
 *         while the pattern is stopped.
 *
 *         The pulses that fell on the tick are given on every tick, for
 *         whatever keeps time with the pattern (a MIDI clock). While the
 *         pattern runs they are the ones its steps are counted in, from
 *         whichever clock it runs from: SEQ_PULSES_PER_STEP to a step on
 *         the internal clock, and that many times the multiply from one
 *         sync edge to the next on the external one. They are given on the
 *         tick they fell, also when a step they bring due begins later (an
 *         early edge, a stall). On the tick of a start no time is counted,
 *         so none falls but the one an edge of the external clock gives.
 *         While the pattern is stopped and no external edge has come since
 *         the stop, the internal clock carries on at the tempo and its
 *         pulses are given; the pattern does not move, and the next start
 *         begins a whole step all the same. Once an edge has come, none
 *         falls.
 *
 *         The LFO in p_in->mod moves three things, each by its own
 *         amount, and each as the LFO reads on the tick the step begins
 *         (see mod.h for how a reading and an amount make a depth). With an
 *         amount of 0, or the LFO at its centre or not valid, nothing is
 *         moved. The note value is shifted by up to 7 places in the scale,
 *         up while the LFO is above its centre and down while it is below,
 *         and kept within 1 to 15, so it never becomes a rest and a rest is
 *         never moved. The gate length is shifted by up to half a step
 *         either way and kept within 1 pulse to a whole step, which ties.
 *         And while the LFO is above its centre a step that holds a note
 *         may drop out, becoming a rest, as often as the depth says: 255
 *         times in 256 at the most. That is drawn from the same random
 *         numbers the engines use, and only when the chance is not 0.
 *
 *         The tempo is SEQ_BPM_MIN plus the tempo reading divided by
 *         SEQ_TEMPO_RAW_PER_BPM (30 to 285 BPM over the valid range). If the
 *         tempo reading is not valid, the last valid one is reused, for the
 *         ratio as for the tempo.
 * @param  p_state  State from seq_init() or the previous tick; updated.
 * @param  p_in     Inputs for this tick.
 * @param  p_out    Receives the outputs; every field is written.
 * @note   Does nothing if any pointer is null.
 */
void seq_tick(seq_state_t *p_state, const seq_inputs_t *p_in, seq_outputs_t *p_out);

#endif /* SEQ_H */
