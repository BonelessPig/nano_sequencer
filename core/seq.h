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

#define SEQ_STEP_COUNT       (16U) // Number of sequencer steps
#define SEQ_NOTE_BITS        (4U)  // Bits per step's note value; must evenly divide 8
#define SEQ_BITS_PER_BYTE    (8U)

// Raw input bytes needed to hold every step's note bits (8 with the values above)
#define SEQ_RAW_BYTE_COUNT   ((SEQ_STEP_COUNT * SEQ_NOTE_BITS) / SEQ_BITS_PER_BYTE)

#define SEQ_NOTE_MAX         ((1U << SEQ_NOTE_BITS) - 1U) // Largest note value (15)
#define SEQ_TEMPO_RAW_PER_MS (4U)    // Tempo reading units per millisecond of delay

/**
 * @brief State the sequencer carries from one tick to the next.
 */
typedef struct
{
    uint16_t last_tempo_raw; // Most recent valid tempo reading, 0 to 1023 (10-bit)
} seq_state_t;

/**
 * @brief One snapshot of the outside world, gathered before a tick.
 */
typedef struct
{
    // Step-note bits packed MSB first: step 0 is the high nibble of byte 0,
    // step 1 the low nibble of byte 0, step 2 the high nibble of byte 1, ...
    uint8_t  raw_steps[SEQ_RAW_BYTE_COUNT];
    bool     b_steps_valid; // false if raw_steps could not be read this tick
    uint16_t tempo_raw;     // Tempo control position, 0 to 1023 (10-bit)
    bool     b_tempo_valid; // false if tempo_raw could not be read this tick
} seq_inputs_t;

/**
 * @brief What the sequencer wants done after a tick.
 */
typedef struct
{
    uint8_t  notes[SEQ_STEP_COUNT]; // Note value per step, 0 to SEQ_NOTE_MAX; all 0 if not valid
    bool     b_notes_valid;         // false if the step inputs were not valid this tick
    uint16_t delay_ms;              // Time to wait before the next tick, in milliseconds
} seq_outputs_t;

/**
 * @brief  Puts the state into its power-on condition (no tempo reading yet,
 *         so the delay is 0 ms until one arrives).
 * @param  p_state  State to initialize. Ignored if null.
 */
void seq_init(seq_state_t *p_state);

/**
 * @brief  Runs one tick: decodes the step notes and works out the delay
 *         before the next tick. Reads *p_state and *p_in, writes *p_state
 *         and *p_out, and has no other effect.
 *
 *         The delay is the tempo reading divided by SEQ_TEMPO_RAW_PER_MS
 *         (0 to 255 ms over the valid range). If the tempo reading is not
 *         valid this tick, the last valid one is reused.
 * @param  p_state  State from seq_init() or the previous tick; updated.
 * @param  p_in     Inputs for this tick.
 * @param  p_out    Receives the outputs; every field is written.
 * @note   Does nothing if any pointer is null.
 */
void seq_tick(seq_state_t *p_state, const seq_inputs_t *p_in, seq_outputs_t *p_out);

#endif /* SEQ_H */
