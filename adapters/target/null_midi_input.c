/**
 * @file   null_midi_input.c
 * @brief  Target implementation of the MIDI input port for the debug and
 *         release builds: nothing is ever received and the USART is left
 *         untouched. The midi build links midi_input.c instead.
 * @author BonelessPig
 *
 * @copyright Copyright (c) 2026
 *
 */
#include "midi_input.h"
#include <stdint.h>
#include "midi_input_port.h"

/**
 * @brief Does nothing; there is no transport to listen to (see
 *        midi_input.h).
 */
void midi_input_init(void)
{
}

/**
 * @brief  Hands over nothing, since nothing is received (see
 *         midi_input_port.h).
 * @param  p_bytes    ignored
 * @param  max_count  ignored
 * @return always 0
 */
uint8_t midi_input_read(uint8_t *p_bytes, uint8_t max_count)
{
    (void)p_bytes;
    (void)max_count;
    return 0U;
}
