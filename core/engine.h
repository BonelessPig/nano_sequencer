#ifndef ENGINE_H
#define ENGINE_H
/**
 * @file   engine.h
 * @brief  The engine interface. An engine decides what each step of the
 *         pattern is: it reads the panel switches its own way and keeps
 *         whatever state it needs to know where it is. Pure logic with no
 *         hardware access; compiles unchanged for the MCU and for a PC.
 * @author BonelessPig
 *
 * Every engine has a state type and two functions, on this pattern (the
 * plain engine, engine_plain.h, is the first):
 *
 *   void engine_<name>_init(engine_<name>_state_t *p_state);
 *       Goes back to the start of the pattern. This is both the power-on
 *       condition and what a reset does.
 *
 *   bool engine_<name>_step(engine_<name>_state_t *p_state,
 *                           const engine_inputs_t *p_in,
 *                           engine_step_t *p_step);
 *       Called once each time a step is due. Fills in *p_step with the step
 *       to play and moves on to the one after it. Returns false, leaving
 *       *p_step alone, if there is no step to play (the pattern has
 *       finished, or a pointer is null).
 *
 * The sequencer owns the clock and decides when a step is due; the engine
 * decides which step it is and what its note value is. The note value still
 * has to go through the note mapping to become a pitch.
 *
 * @copyright Copyright (c) 2026
 *
 */
#include <stdint.h>
#include "address.h"

/**
 * @brief What an engine is given when a step is due.
 */
typedef struct
{
    const uint8_t          *p_raw_steps; // Panel bits, PANEL_RAW_BYTE_COUNT bytes
                                         // (see panel.h); the latest readable
                                         // ones if the panel could not be read
    const address_config_t *p_address;   // The addressing controls
} engine_inputs_t;

/**
 * @brief One step, as an engine hands it back.
 */
typedef struct
{
    uint8_t step; // Panel step being played, 0 to PANEL_STEP_COUNT - 1
    uint8_t note; // Its note value: 0 is a rest, 1 to 15 are notes of the scale
} engine_step_t;

#endif /* ENGINE_H */
