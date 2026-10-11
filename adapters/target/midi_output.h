#ifndef MIDI_OUTPUT_H
#define MIDI_OUTPUT_H
/**
 * @file   midi_output.h
 * @brief  Target-private interface of the MIDI output adapter: its start-up.
 *         The port functions are declared in midi_output_port.h. Two source
 *         files implement both headers and the build links exactly one of
 *         them: midi_output.c (midi build, bytes over USART0) or
 *         null_midi_output.c (debug and release builds, discards
 *         everything).
 * @author BonelessPig
 *
 * @copyright Copyright (c) 2026
 *
 */

/**
 * @brief  Sets up the transport and empties the queue. Call once, before
 *         any midi_output_port.h function. It cannot fail.
 * @note   Call with interrupts off, which they are until timebase_init().
 *         In the midi build this takes over USART0 and with it PD0 and PD1;
 *         the null adapter touches nothing.
 */
void midi_output_init(void);

#endif /* MIDI_OUTPUT_H */
