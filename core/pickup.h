#ifndef PICKUP_H
#define PICKUP_H
/**
 * @file   pickup.h
 * @brief  Pickup: lets one pot set a stored value without making it jump.
 *         Pure logic with no hardware access; compiles unchanged for the MCU
 *         and for a PC.
 * @author BonelessPig
 *
 * A pot that is shared between several values is seldom where the value it
 * has just been given to was left. So the pot starts detached: the value
 * stays as it is until the pot has been turned to meet it. That is a reading
 * within 8 of the value, or two readings in a row with the value between
 * them. From then on the pot is attached and the value is its reading, until
 * pickup_release() detaches it again.
 *
 * @copyright Copyright (c) 2026
 *
 */
#include <stdbool.h>
#include <stdint.h>

/**
 * @brief State the pickup carries from one reading to the next.
 */
typedef struct
{
    bool     b_attached; // true while the value follows the pot
    bool     b_known;    // false until the first reading has been taken
    uint16_t last;       // The reading before this one
} pickup_state_t;

/**
 * @brief  Detaches the pot and forgets its last reading.
 * @param  p_state  State to initialize. Ignored if null.
 */
void pickup_init(pickup_state_t *p_state);

/**
 * @brief  Detaches the pot, for when it is given another value to set. Its
 *         last reading is kept, since the pot has not moved.
 * @param  p_state  State from pickup_init() or an earlier call; updated.
 *                  Ignored if null.
 */
void pickup_release(pickup_state_t *p_state);

/**
 * @brief  Takes one reading of the pot. If the pot is attached, or this
 *         reading attaches it, the value becomes the reading; otherwise the
 *         value is left as it is. Reads and writes *p_state and *p_value and
 *         has no other effect.
 * @param  p_state  State from pickup_init() or an earlier call; updated.
 * @param  reading  The pot's position, in the units of the value. Any
 *                  16-bit number.
 * @param  p_value  The value the pot is setting; written only while
 *                  attached.
 * @return true if the pot is attached after this reading. false if either
 *         pointer is null, and nothing is then written.
 */
bool pickup_update(pickup_state_t *p_state, uint16_t reading, uint16_t *p_value);

#endif /* PICKUP_H */
