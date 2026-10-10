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

#endif /* ADDRESS_H */
