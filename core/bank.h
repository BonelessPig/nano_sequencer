#ifndef BANK_H
#define BANK_H
/**
 * @file   bank.h
 * @brief  A bank of amounts set by one pot: a selector says which of them
 *         the pot is setting. Pure logic with no hardware access; compiles
 *         unchanged for the MCU and for a PC.
 * @author BonelessPig
 *
 * Each slot holds an amount, 0 to 1023 where the pot's readings are. The pot
 * sets the selected slot, through a pickup (see pickup.h): after the
 * selector moves, the pot changes nothing until it has been turned to meet
 * the amount the newly selected slot holds. Passing through a slot on the
 * way to another therefore changes nothing, and neither does a selector that
 * reads a wrong slot for a moment between two positions.
 *
 * Every amount is 0 after bank_init(), but one: the home slot takes the
 * pot's first valid reading, wherever the selector stands, so that what it
 * sets (a tempo, say) starts where the pot is and not at the bottom of its
 * range. Nothing is kept through a power cycle.
 *
 * The bank gives the amounts no meaning. The caller says what each slot is.
 *
 * @copyright Copyright (c) 2026
 *
 */
#include <stdbool.h>
#include <stdint.h>
#include "pickup.h"

#define BANK_SLOT_COUNT (8U) // Number of amounts in the bank

/**
 * @brief State the bank carries from one tick to the next.
 */
typedef struct
{
    uint16_t       amount[BANK_SLOT_COUNT]; // Each slot's amount, 0 to 1023
    pickup_state_t pickup;                  // Whether the pot has met the selected one
    uint8_t        selected;                // The slot the pot is setting
    uint8_t        home;                    // The slot that takes the first reading
    bool           b_homed;                 // true once it has
} bank_state_t;

/**
 * @brief The controls, as last read.
 */
typedef struct
{
    uint8_t  selected;  // Selector position: the slot the pot is to set. A
                        // value of BANK_SLOT_COUNT or more wraps round (8 is 0)
    uint16_t reading;   // The pot's position, 0 to 1023. A larger value is
                        // taken as 1023
    bool     b_reading_valid; // false if reading could not be read; the
                        // amounts are then left as they are
} bank_inputs_t;

/**
 * @brief  Empties the bank: every amount 0, slot 0 selected, the pot
 *         detached, and the home slot waiting for its reading.
 * @param  p_state    State to initialize. Ignored if null.
 * @param  home_slot  The slot that takes the pot's first valid reading, 0 to
 *                    BANK_SLOT_COUNT - 1. With a larger value none does.
 */
void bank_init(bank_state_t *p_state, uint8_t home_slot);

/**
 * @brief  Runs one tick: follows the selector and lets the pot set the
 *         selected slot once it has met it. Reads *p_state and *p_in, writes
 *         *p_state, and has no other effect. Calling it with the same
 *         inputs again changes nothing, so it may run on every tick with
 *         controls that are read less often.
 * @param  p_state  State from bank_init() or the previous tick; updated.
 * @param  p_in     The controls.
 * @note   Does nothing if either pointer is null.
 */
void bank_tick(bank_state_t *p_state, const bank_inputs_t *p_in);

/**
 * @brief  Gives one slot's amount.
 * @param  p_state  State from bank_init() or bank_tick().
 * @param  slot     The slot. A value of BANK_SLOT_COUNT or more wraps round.
 * @return The amount, 0 to 1023. 0 if p_state is null.
 */
uint16_t bank_amount(const bank_state_t *p_state, uint8_t slot);

#endif /* BANK_H */
