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
#include "clock.h"

#define SEQ_STEP_COUNT       (16U) // Number of sequencer steps
#define SEQ_NOTE_BITS        (4U)  // Bits per step's note value; must evenly divide 8
#define SEQ_BITS_PER_BYTE    (8U)

// Raw input bytes needed to hold every step's note bits (8 with the values above)
#define SEQ_RAW_BYTE_COUNT   ((SEQ_STEP_COUNT * SEQ_NOTE_BITS) / SEQ_BITS_PER_BYTE)

#define SEQ_NOTE_MAX         ((1U << SEQ_NOTE_BITS) - 1U) // Largest note value (15)

// A step is a sixteenth note: four to the quarter note, 24 clock pulses each
#define SEQ_STEPS_PER_BEAT   (4U)
#define SEQ_PULSES_PER_STEP  (CLOCK_PPQN / SEQ_STEPS_PER_BEAT)

// Tempo: the control's 10-bit range maps linearly onto 30 to 285 beats per
// minute, so there is a floor at the slow end and no way to stop the clock.
#define SEQ_BPM_MIN           (30U) // Tempo with the control at 0
#define SEQ_TEMPO_RAW_PER_BPM (4U)  // Tempo reading units per beat per minute

/**
 * @brief State the sequencer carries from one tick to the next.
 */
typedef struct
{
    clock_state_t clock;          // Turns elapsed ticks into pulses at the tempo
    uint16_t      last_tempo_raw; // Most recent valid tempo reading, 0 to 1023 (10-bit)
    uint16_t      step_pulses;    // Pulses since the last step began; a step is due
                                  // at SEQ_PULSES_PER_STEP
    uint8_t       current_step;   // Step that begins next, 0 to SEQ_STEP_COUNT - 1
} seq_state_t;

/**
 * @brief One snapshot of the outside world, gathered before a tick.
 */
typedef struct
{
    // Step-note bits packed MSB first: step 0 is the high nibble of byte 0,
    // step 1 the low nibble of byte 0, step 2 the high nibble of byte 1, ...
    uint8_t  raw_steps[SEQ_RAW_BYTE_COUNT];
    bool     b_steps_valid; // false if raw_steps could not be read
    uint16_t tempo_raw;     // Tempo control position, 0 to 1023 (10-bit)
    bool     b_tempo_valid; // false if tempo_raw could not be read
    uint8_t  elapsed_ticks; // Timebase ticks (milliseconds) since the previous tick
} seq_inputs_t;

/**
 * @brief What the sequencer wants done after a tick.
 */
typedef struct
{
    bool    b_step_started; // true on the tick a step begins; the fields below
                            // describe that step, and are 0 and false otherwise
    uint8_t step;           // Step that began, 0 to SEQ_STEP_COUNT - 1
    uint8_t note;           // Note value of that step, 0 to SEQ_NOTE_MAX; 0 if not valid
    bool    b_note_valid;   // false if the step inputs were not valid on that tick
} seq_outputs_t;

/**
 * @brief  Puts the state into its power-on condition: step 0 begins on the
 *         first tick, and there is no tempo reading yet, so the tempo is
 *         SEQ_BPM_MIN until one arrives.
 * @param  p_state  State to initialize. Ignored if null.
 */
void seq_init(seq_state_t *p_state);

/**
 * @brief  Runs one tick: moves the clock on by the elapsed time and, if that
 *         brings a step due, begins it and decodes its note. Reads *p_state
 *         and *p_in, writes *p_state and *p_out, and has no other effect.
 *
 *         A step lasts SEQ_PULSES_PER_STEP clock pulses, which is 15000 / BPM
 *         milliseconds. Steps follow one another, wrapping from the last back
 *         to step 0. The first tick after seq_init() begins step 0 whatever
 *         the elapsed time, and that tick's time counts towards step 1, so
 *         the first step is one tick shorter than the rest.
 *
 *         The note is taken from the inputs of the tick on which its step
 *         begins, so a change on the panel is heard the next time its step
 *         comes round. A step begins on time even if the step inputs are not
 *         valid (its note is then 0), so a failed read does not shift the
 *         pattern in time. At most one step begins per tick: after a stall
 *         of more than a step, the steps owed begin on successive ticks.
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
