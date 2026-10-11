#ifndef MIDI_OUTPUT_PORT_H
#define MIDI_OUTPUT_PORT_H
/**
 * @file   midi_output_port.h
 * @brief  Port: MIDI out, as a stream of bytes. The core decides which
 *         bytes; the adapter owns the transport (a serial line at 31250
 *         baud in the target's midi build; the other builds discard every
 *         byte). No function here blocks or can fail: bytes are queued, and
 *         midi_output_poll() sends them one at a time.
 * @author BonelessPig
 *
 * @copyright Copyright (c) 2026
 *
 */
#include <stdint.h>

/**
 * @brief  Tells how many bytes midi_output_write() can queue at this moment.
 * @return Free space in the queue, in bytes, 0 to 255. An adapter that
 *         discards everything always gives 255.
 * @note   Not ISR-safe. Never blocks. The room only grows between calls, as
 *         midi_output_poll() sends, until the next midi_output_write().
 */
uint8_t midi_output_room(void);

/**
 * @brief  Queues bytes for midi_output_poll() to send, in the order given.
 *         The caller is expected never to write more than
 *         midi_output_room() last gave: bytes that do not fit are dropped,
 *         the last of them first, which can cut a message short.
 * @param  p_bytes  The bytes to send; null queues nothing.
 * @param  count    How many, 0 to 255.
 * @note   Not ISR-safe. Never blocks: nothing is sent here. A queued byte
 *         leaves once every byte ahead of it has gone, at 320 microseconds
 *         a byte on the target's midi build.
 */
void midi_output_write(const uint8_t *p_bytes, uint8_t count);

/**
 * @brief  Sends at most one queued byte, if the transport is ready for it.
 *         Call on every pass of the main loop.
 * @note   Not ISR-safe. Never blocks. On the target's midi build one byte
 *         takes 320 microseconds at 31250 baud, so at most three leave in a
 *         tick of one millisecond, and a three-byte message is gone about
 *         1 ms after it reached the head of the queue.
 */
void midi_output_poll(void);

#endif /* MIDI_OUTPUT_PORT_H */
