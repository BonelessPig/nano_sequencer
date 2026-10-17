/**
 * @file   midi_output.c
 * @brief  Target implementation of the MIDI output port for the midi build:
 *         bytes over USART0 at 31250 baud, 8 data bits, no parity, 1 stop
 *         bit. The debug and release builds link null_midi_output.c
 *         instead. Nothing here waits for the UART. Bytes are written into
 *         a queue, and midi_output_poll() sends one queued byte each time
 *         the transmitter has room.
 * @author BonelessPig
 *
 * @copyright Copyright (c) 2026
 *
 */
#include "midi_output.h"
#include <stddef.h>
#include <stdint.h>
#include "midi_output_port.h"
#include "atmega328p_usart_regs.h"

// F_CPU is provided by the build (see Makefile / -DF_CPU) rather than defined
// here, so there is a single source of truth for the clock speed.
#ifndef F_CPU
#error "F_CPU must be defined by the build (e.g. -DF_CPU=16000000UL)"
#endif

#define BAUD (31250UL) // The MIDI baud rate

// Calculated the way serial_logger.c does it, with the same register writes
// and bit positions, so that the two builds that use the USART use the same
// definitions of atmega328p_usart_regs.h. In double-speed mode 31250 baud is
// 16 MHz divided by exactly 64, a register value of 63. The division rounds
// to the nearest divisor rather than down.
#define UBRR_SAMPLES_PER_BIT (8UL) // Double-speed mode samples each bit 8 times
// CPU clocks per bit at a divisor of 1
#define UBRR_CLOCKS_PER_BIT  (UBRR_SAMPLES_PER_BIT * BAUD)
// USART Baud Rate Register value
#define UBRR_VALUE \
    ((((F_CPU) + (UBRR_CLOCKS_PER_BIT / 2UL)) / UBRR_CLOCKS_PER_BIT) - 1UL)
#define BYTE_RANGE  (256UL) // Number of values one byte can hold
#define UBRRH_VALUE ((uint8_t)(UBRR_VALUE / BYTE_RANGE)) // High byte of UBRR value
#define UBRRL_VALUE ((uint8_t)(UBRR_VALUE % BYTE_RANGE)) // Low  byte of UBRR value

// Room for four ticks of the most the core sends in one (8 bytes), which is
// more than the three bytes a tick the line can carry away
#define MIDI_QUEUE_SIZE (32U)

static uint8_t g_midi_queue[MIDI_QUEUE_SIZE]; // Bytes waiting to be sent, as a ring
static uint8_t g_midi_tail  = 0U;             // Index of the next byte to send
static uint8_t g_midi_count = 0U;             // Bytes waiting, 0 to MIDI_QUEUE_SIZE



/**
 * @brief Tells how much of the queue is free.
 * @return free space in bytes, 0 to MIDI_QUEUE_SIZE
 */
static uint8_t queue_room(void)
{
    return (uint8_t)(MIDI_QUEUE_SIZE - g_midi_count);
}



/**
 * @brief Sets up USART0 and empties the queue (see midi_output.h).
 */
void midi_output_init(void)
{
    UBRR0H = UBRRH_VALUE; // Set baud rate high byte
    UBRR0L = UBRRL_VALUE; // Set baud rate low byte

    // Set these explicitly rather than relying on reset defaults, in case a
    // bootloader left them changed
    UCSR0A = (uint8_t)(1U << U2X0); // Double speed, no multi-processor mode
    // Frame format: 8 data bits, no parity, 1 stop bit
    UCSR0C = (uint8_t)((1U << UCSZ01) | (1U << UCSZ00));

    // The receiver is enabled as the logger enables it; midi_input.c reads it
    UCSR0B = (uint8_t)((1U << RXEN0) | (1U << TXEN0));

    g_midi_tail  = 0U; // Empty the queue
    g_midi_count = 0U;
}



/**
 * @brief Gives the free space in the queue (see midi_output_port.h).
 * @return bytes that can be queued now, 0 to MIDI_QUEUE_SIZE
 */
uint8_t midi_output_room(void)
{
    return queue_room();
}



/**
 * @brief Queues as many of the bytes as fit and drops the rest (see
 *        midi_output_port.h).
 * @param p_bytes bytes to queue; null queues nothing
 * @param count how many
 */
void midi_output_write(const uint8_t *p_bytes, uint8_t count)
{
    if (NULL != p_bytes)
    {
        const uint8_t room     = queue_room();
        const uint8_t accepted = (count < room) ? count : room;

        for (uint8_t i = 0U; i < accepted; i++)
        {
            const uint8_t index =
                (uint8_t)((g_midi_tail + g_midi_count) % MIDI_QUEUE_SIZE);

            g_midi_queue[index] = p_bytes[i];
            g_midi_count++;
        }
    }
}



/**
 * @brief Sends the next queued byte if there is one and the transmitter has
 *        room for it (see midi_output_port.h). Never waits.
 */
void midi_output_poll(void)
{
    if (g_midi_count > 0U)
    {
        if (0U != (UCSR0A & (1U << UDRE0))) // Transmit buffer empty
        {
            UDR0 = g_midi_queue[g_midi_tail]; // Put data into buffer, sends the data
            g_midi_tail = (uint8_t)((g_midi_tail + 1U) % MIDI_QUEUE_SIZE);
            g_midi_count--;
        }
    }
}
