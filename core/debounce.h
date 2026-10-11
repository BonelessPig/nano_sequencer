#ifndef DEBOUNCE_H
#define DEBOUNCE_H
/**
 * @file   debounce.h
 * @brief  Debounce: the settled level of a button or switch whose contacts
 *         bounce. Pure logic with no hardware access; compiles unchanged
 *         for the MCU and for a PC.
 * @author BonelessPig
 *
 * The first reading after debounce_init() is taken as it stands, so a switch
 * that is already on at power-on counts as on from the first tick. After
 * that the level changes only once the raw reading has differed from it for
 * 20 ticks (milliseconds) without a break.
 *
 * @copyright Copyright (c) 2026
 *
 */
#include <stdbool.h>
#include <stdint.h>

/**
 * @brief State one input carries from one tick to the next.
 */
typedef struct
{
    bool    b_known; // false until the first reading has been taken
    bool    b_level; // The settled level
    uint8_t ticks;   // Ticks the raw reading has differed from it, 0 to 19
} debounce_state_t;

/**
 * @brief  Forgets the level: the next reading is taken as it stands.
 * @param  p_state  State to initialize. Ignored if null.
 */
void debounce_init(debounce_state_t *p_state);

/**
 * @brief  Takes one raw reading and gives the settled level. Reads and
 *         writes *p_state and has no other effect.
 * @param  p_state        State from debounce_init() or the previous call;
 *                        updated.
 * @param  b_raw          The input as read now, bounce and all.
 * @param  elapsed_ticks  Ticks (milliseconds) since the previous call, 0 to
 *                        255. The reading is taken to have stood for all of
 *                        them.
 * @return The settled level. false if p_state is null.
 */
bool debounce_update(debounce_state_t *p_state, bool b_raw, uint8_t elapsed_ticks);

#endif /* DEBOUNCE_H */
