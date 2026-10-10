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
#include "engine_plain.h"
#include "gate.h"
#include "note_map.h"
#include "panel.h"
#include "pitch_cal.h"

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
    clock_state_t        clock;          // Turns elapsed ticks into pulses at the tempo
    uint16_t             last_tempo_raw; // Most recent valid tempo reading, 0 to 1023
    uint16_t             step_pulses;    // Pulses since the last step was due; the
                                         // next is due at SEQ_PULSES_PER_STEP
    engine_plain_state_t engine;         // Where the engine has got to in the pattern
    gate_state_t         gate;           // How much of the current note's gate is left
    uint16_t             cv;             // Pitch CV value of the last note played
} seq_state_t;

/**
 * @brief One snapshot of the outside world, gathered before a tick.
 */
typedef struct
{
    // The panel's step bits, packed as panel.h describes
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
} seq_outputs_t;

/**
 * @brief  Puts the state into its power-on condition: the first step of the
 *         pattern begins on the first tick, and there is no tempo reading
 *         yet, so the tempo is SEQ_BPM_MIN until one arrives. The pitch CV
 *         value is 0 until the first note.
 * @param  p_state  State to initialize. Ignored if null.
 */
void seq_init(seq_state_t *p_state);

/**
 * @brief  Runs one tick: moves the clock on by the elapsed time and, if that
 *         brings a step due, begins it and works out its pitch. Reads
 *         *p_state and *p_in, writes *p_state and *p_out, and has no other
 *         effect.
 *
 *         A step lasts SEQ_PULSES_PER_STEP clock pulses, which is 15000 / BPM
 *         milliseconds. The first tick after seq_init() begins the first
 *         step whatever the elapsed time, and that tick's time counts
 *         towards the second, so the first step is one tick shorter than
 *         the rest.
 *
 *         Which step begins is up to the engine (the plain engine, see
 *         engine_plain.h) and the addressing controls in p_in->address (see
 *         address.h). When a one-shot pattern has finished, the clock keeps
 *         running and steps keep falling due, but none begins.
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
 *         The tempo is SEQ_BPM_MIN plus the tempo reading divided by
 *         SEQ_TEMPO_RAW_PER_BPM (30 to 285 BPM over the valid range). If the
 *         tempo reading is not valid, the last valid one is reused.
 * @param  p_state  State from seq_init() or the previous tick; updated.
 * @param  p_in     Inputs for this tick.
 * @param  p_out    Receives the outputs; every field is written.
 * @note   Does nothing if any pointer is null.
 */
void seq_tick(seq_state_t *p_state, const seq_inputs_t *p_in, seq_outputs_t *p_out);

#endif /* SEQ_H */
