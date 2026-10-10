/**
 * @file   panel.c
 * @brief  Front panel layout implementation. Pure logic: no hardware access.
 * @author BonelessPig
 *
 * @copyright Copyright (c) 2026
 *
 */
#include "panel.h"
#include <stddef.h>

#define VALUE_MASK ((uint8_t)PANEL_VALUE_MAX)



uint8_t panel_step_value(const uint8_t *p_raw_steps, uint8_t step)
{
    uint8_t value = 0U;

    if (NULL != p_raw_steps)
    {
        const uint8_t wrapped    = (uint8_t)(step % PANEL_STEP_COUNT);
        const uint8_t bit_pos    = (uint8_t)(wrapped * PANEL_VALUE_BITS);
        const uint8_t byte_index = (uint8_t)(bit_pos / PANEL_BITS_PER_BYTE);
        const uint8_t bit_offset = (uint8_t)(bit_pos % PANEL_BITS_PER_BYTE);
        const uint8_t shift      =
            (uint8_t)(PANEL_BITS_PER_BYTE - PANEL_VALUE_BITS - bit_offset);

        value = (uint8_t)((uint8_t)(p_raw_steps[byte_index] >> shift) & VALUE_MASK);
    }

    return value;
}
