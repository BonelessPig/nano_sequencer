#ifndef MIDI_INPUT_PORT_H
#define MIDI_INPUT_PORT_H
/**
 * @file   midi_input_port.h
 * @brief  Port: MIDI in, as a stream of bytes. The adapter owns the
 *         transport (a serial line at 31250 baud in the target's midi
 *         build; the other builds never receive anything) and keeps what
 *         arrives between two calls; the core decides what the bytes mean.
 * @author BonelessPig
 *
 * @copyright Copyright (c) 2026
 *
 */
#include <stdint.h>

/**
 * @brief  Hands over the bytes received since the previous call, oldest
 *         first, and forgets them. Call once on every tick. A byte that
 *         arrived damaged is never among them, nor one that came while
 *         there was no room to keep it (the target's midi build keeps 15,
 *         against the 3 a tick the line can carry): both are dropped and
 *         nothing says so.
 * @param  p_bytes    Receives the bytes. Null takes none and gives 0.
 * @param  max_count  The most to hand over, 0 to 255. Any beyond it are
 *                    kept for the next call.
 * @return How many bytes were written to p_bytes, 0 to max_count. An
 *         adapter that receives nothing always gives 0.
 * @note   Not ISR-safe. Never blocks.
 */
uint8_t midi_input_read(uint8_t *p_bytes, uint8_t max_count);

#endif /* MIDI_INPUT_PORT_H */
