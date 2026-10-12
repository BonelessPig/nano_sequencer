/**
 * @file   bank.c
 * @brief  Bank implementation. Pure logic: no hardware access.
 * @author BonelessPig
 *
 * @copyright Copyright (c) 2026
 *
 */
#include "bank.h"
#include <stddef.h>

// The most an amount can be: the top of the pot's range
#define AMOUNT_MAX (1023U)

// An amount is 10 bits and a level 8
#define AMOUNT_PER_LEVEL (4U)



void bank_init(bank_state_t *p_state, uint8_t home_slot)
{
    if (NULL != p_state)
    {
        for (uint8_t slot = 0U; slot < BANK_SLOT_COUNT; slot++)
        {
            p_state->amount[slot] = 0U;
        }
        pickup_init(&p_state->pickup);
        p_state->selected = 0U;
        p_state->home     = home_slot;
        p_state->b_homed  = false;
    }
}



void bank_tick(bank_state_t *p_state, const bank_inputs_t *p_in)
{
    if ((NULL != p_state) && (NULL != p_in))
    {
        const uint8_t selected = (uint8_t)(p_in->selected % BANK_SLOT_COUNT);
        uint16_t      reading  = p_in->reading;

        if (reading > AMOUNT_MAX)
        {
            reading = (uint16_t)AMOUNT_MAX;
        }

        if (selected != p_state->selected)
        {
            p_state->selected = selected;
            pickup_release(&p_state->pickup);
        }

        if (p_in->b_reading_valid)
        {
            if (!p_state->b_homed)
            {
                p_state->b_homed = true;
                if (p_state->home < BANK_SLOT_COUNT)
                {
                    p_state->amount[p_state->home] = reading;
                }
            }
            (void)pickup_update(&p_state->pickup, reading, &p_state->amount[selected]);
        }
    }
}



void bank_set(bank_state_t *p_state, uint8_t slot, uint16_t amount)
{
    if (NULL != p_state)
    {
        const uint8_t wrapped = (uint8_t)(slot % BANK_SLOT_COUNT);

        p_state->amount[wrapped] = (amount > AMOUNT_MAX) ? (uint16_t)AMOUNT_MAX : amount;
        if (wrapped == p_state->selected)
        {
            pickup_release(&p_state->pickup);
        }
    }
}



uint16_t bank_amount(const bank_state_t *p_state, uint8_t slot)
{
    uint16_t amount = 0U;

    if (NULL != p_state)
    {
        amount = p_state->amount[slot % BANK_SLOT_COUNT];
    }

    return amount;
}



uint8_t bank_level(const bank_state_t *p_state)
{
    uint8_t level = 0U;

    if (NULL != p_state)
    {
        // At most 1023 / 4, so it fits in a byte
        level = (uint8_t)(p_state->amount[p_state->selected] / AMOUNT_PER_LEVEL);
    }

    return level;
}
