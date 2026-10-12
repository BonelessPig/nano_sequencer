#ifndef ADDRESS_H
#define ADDRESS_H
/**
 * @file   address.h
 * @brief  Step addressing: which panel step plays next, given a direction, a
 *         first and a last step, and whether the pattern loops or plays once.
 *         Pure logic with no hardware access; compiles unchanged for the MCU
 *         and for a PC.
 * @author BonelessPig
 *
 * The steps from the first to the last are the range. If the first step is
 * the higher of the two, the range runs through step 15 and round to step 0
 * (first 14, last 1 is the four steps 14, 15, 0, 1), so every pair of
 * settings is a valid range of 1 to 16 steps.
 *
 * One cycle is one pass over the range in the chosen direction:
 *   - forward:   first to last.
 *   - reverse:   last to first.
 *   - pendulum:  first to last and back, with each end played once per cycle
 *                (0 1 2 3 2 1, then 0 again). A one-step range has a
 *                one-step cycle.
 *
 * @copyright Copyright (c) 2026
 *
 */
#include <stdbool.h>
#include <stdint.h>

/**
 * @brief The order the steps of the range are played in.
 */
typedef enum
{
    ADDRESS_FORWARD = 0, // First step to last, then the first again
    ADDRESS_REVERSE,     // Last step to first, then the last again
    ADDRESS_PENDULUM     // First to last and back again
} address_direction_t;

/**
 * @brief The addressing controls, as they are set when a step is asked for.
 */
typedef struct
{
    address_direction_t direction;  // An unknown value plays forward
    uint8_t             first_step; // 0 to PANEL_STEP_COUNT - 1; larger values wrap
    uint8_t             last_step;  // 0 to PANEL_STEP_COUNT - 1; larger values wrap
    bool                b_one_shot; // true: stop at the end of one cycle
} address_config_t;

/**
 * @brief State the addressing carries from one step to the next.
 */
typedef struct
{
    uint8_t position;   // Steps played so far in this cycle, 0 to 29
    bool    b_finished; // true once a one-shot cycle has been played in full
} address_state_t;

/**
 * @brief How far to move a place round the places it is one of, and which
 *        way.
 */
typedef struct
{
    uint8_t depth;      // 0 for nothing, up to 255 for half of the places
    bool    b_negative; // true to move back, towards the first place
} address_offset_t;

/**
 * @brief  Goes back to the start of the cycle, ready to play again. This is
 *         both the power-on condition and what a reset does.
 * @param  p_state  State to initialize. Ignored if null.
 */
void address_init(address_state_t *p_state);

/**
 * @brief  Gives the step to play now and moves on to the one after it. Reads
 *         *p_config, reads and writes *p_state, and has no other effect.
 *
 *         The state counts steps played in the current cycle, not a step
 *         number, so when the direction or the range changes mid-cycle the
 *         count carries over into the new cycle; a count beyond the end of
 *         the new cycle starts it again.
 *
 *         With b_one_shot set, the call after the last step of a cycle
 *         returns false, and so does every call until address_init() or
 *         until b_one_shot is cleared, which starts the next cycle.
 * @param  p_state   State from address_init() or the previous call; updated.
 * @param  p_config  The controls as they are now.
 * @param  p_step    Receives the step to play, 0 to PANEL_STEP_COUNT - 1.
 *                   Written only when the return value is true.
 * @return true if there is a step to play; false if a one-shot cycle has
 *         finished, or if any pointer is null.
 */
bool address_next(address_state_t *p_state, const address_config_t *p_config,
                  uint8_t *p_step);

/**
 * @brief  Moves a place round a ring of places: by half of them, rounded
 *         down, at a depth of 255 and in proportion below that, rounded to
 *         the nearest whole number. Going past either end comes round to
 *         the other. Reads *p_offset and has no other effect.
 * @param  place     The place to move, 0 to count - 1; larger values wrap.
 * @param  count     Places in the ring, 1 to 255.
 * @param  p_offset  How far to move it, and which way.
 * @return The place moved to, 0 to count - 1. The place as it was given if
 *         count is 0 or p_offset is null.
 */
uint8_t address_turn(uint8_t place, uint8_t count, const address_offset_t *p_offset);

/**
 * @brief  Moves a step to another of the range, with the places of the
 *         range as the ring of address_turn(): they are counted from the
 *         first step towards the last, whatever the direction, so nothing
 *         outside the range is given and a range of one step moves nothing.
 *         Reads *p_config and *p_offset and has no other effect.
 * @param  p_config  The controls as they are now, for the first and the
 *                   last step.
 * @param  step      The step to move, 0 to PANEL_STEP_COUNT - 1; larger
 *                   values wrap. One outside the range is taken round into
 *                   it.
 * @param  p_offset  How far to move it, and which way.
 * @return The step moved to, 0 to PANEL_STEP_COUNT - 1. The step as it was
 *         given if either pointer is null.
 */
uint8_t address_offset_step(const address_config_t *p_config, uint8_t step,
                            const address_offset_t *p_offset);

#endif /* ADDRESS_H */
