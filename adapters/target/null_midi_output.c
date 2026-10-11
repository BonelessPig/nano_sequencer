/**
 * @file   null_midi_output.c
 * @brief  Target implementation of the MIDI output port for the debug and
 *         release builds: every byte is discarded and the USART is left
 *         untouched. The midi build links midi_output.c instead.
 * @author BonelessPig
 *
 * @copyright Copyright (c) 2026
 *
 */
#include "midi_output.h"
#include <stdint.h>
#include "midi_output_port.h"



/**
 * @brief Does nothing; there is no transport to set up (see midi_output.h).
 */
void midi_output_init(void)
{
}



/**
 * @brief Gives the most room there can be, since nothing is kept (see
 *        midi_output_port.h).
 * @return always 255
 */
uint8_t midi_output_room(void)
{
    return UINT8_MAX;
}



/**
 * @brief Discards the bytes (see midi_output_port.h).
 * @param p_bytes ignored
 * @param count ignored
 */
void midi_output_write(const uint8_t *p_bytes, uint8_t count)
{
    (void)p_bytes;
    (void)count;
}



/**
 * @brief Does nothing; no byte is ever queued (see midi_output_port.h).
 */
void midi_output_poll(void)
{
}
