#ifndef GATE_H
#define GATE_H
/**
 * @file   gate.h
 * @brief  The gate: how long a note is held, counted in clock pulses. Pure
 *         logic with no hardware access; compiles unchanged for the MCU and
 *         for a PC.
 * @author BonelessPig
 *
 * A gate opens when a step with a note begins and closes a set number of
 * pulses later. Every step boundary ends the gate before it: a note opens a
 * new one there, and a rest (or no step at all) leaves it closed. So a length
 * of a whole step or more never closes between two notes, which ties them
 * into one long gate, and a rest after a tied note still ends it.
 *
 * @copyright Copyright (c) 2026
 *
 */
#include <stdbool.h>
#include <stdint.h>

/**
 * @brief The gate control, read when a step begins.
 */
typedef struct
{
    uint8_t length; // Pulses a note's gate stays open, 0 to 255. A step is 24
                    // pulses: 12 is half a step, 24 or more ties to the next
                    // note, and 0 never opens the gate
} gate_config_t;

/**
 * @brief State the gate carries from one tick to the next.
 */
typedef struct
{
    uint8_t pulses_left; // Pulses until the gate closes; 0 when it is closed
} gate_state_t;

/**
 * @brief  Closes the gate.
 * @param  p_state  State to initialize. Ignored if null.
 */
void gate_init(gate_state_t *p_state);

/**
 * @brief  Marks a step boundary: ends the gate in progress, and opens a new
 *         one of p_config->length pulses if a note begins. Reads *p_config,
 *         writes *p_state, and has no other effect.
 * @param  p_state   State from gate_init() or an earlier call; updated.
 *                   Nothing is done if it is null.
 * @param  p_config  The gate control. If null, the gate is left closed.
 * @param  b_note    true if a note begins at this boundary; false for a
 *                   rest, a step whose note is not known, or no step.
 */
void gate_begin(gate_state_t *p_state, const gate_config_t *p_config, bool b_note);

/**
 * @brief  Moves the gate on by a number of clock pulses and reports its
 *         level. Reads and writes *p_state and has no other effect.
 * @param  p_state  State from gate_init() or an earlier call; updated.
 * @param  pulses   Clock pulses since the previous call, or since
 *                  gate_begin() if that came later. 0 only reports the level.
 * @return true while the gate is open: fewer pulses have passed than its
 *         length. false once it has closed, and if p_state is null.
 */
bool gate_advance(gate_state_t *p_state, uint16_t pulses);

#endif /* GATE_H */
