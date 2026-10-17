#ifndef MIDI_INPUT_H
#define MIDI_INPUT_H
/**
 * @file   midi_input.h
 * @brief  Target-private interface of the MIDI input adapter: its start-up.
 *         The port function is declared in midi_input_port.h. Two source
 *         files implement both headers and the build links exactly one of
 *         them: midi_input.c (midi build, bytes from USART0) or
 *         null_midi_input.c (debug and release builds, which receive
 *         nothing).
 * @author BonelessPig
 *
 * @copyright Copyright (c) 2026
 *
 */

/**
 * @brief  Forgets any byte kept so far and starts receiving. Call once,
 *         after midi_output_init(), which sets the USART up, and before
 *         midi_input_read(). It cannot fail.
 * @note   Call with interrupts off, which they are until timebase_init():
 *         in the midi build this turns on the receive interrupt. The null
 *         adapter touches nothing.
 */
void midi_input_init(void);

#endif /* MIDI_INPUT_H */
