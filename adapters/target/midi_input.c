/**
 * @file   midi_input.c
 * @brief  Target implementation of the MIDI input port for the midi build:
 *         bytes from USART0, which midi_output.c sets up at 31250 baud with
 *         its receiver on. The debug and release builds link
 *         null_midi_input.c instead. An interrupt takes each byte as it
 *         arrives, 320 microseconds apart at the closest, and puts it in a
 *         ring that midi_input_read() empties once a tick.
 * @author BonelessPig
 *
 * @copyright Copyright (c) 2026
 *
 */
#include "midi_input.h"
#include <stddef.h>
#include <stdint.h>
#include "atmega328p_usart_rx_regs.h"
#include "midi_input_port.h"

// One slot is always left empty, so the ring holds 15 bytes: five ticks of
// the three a tick the line can carry. A power of two, so that an index
// wraps with a mask
#define RX_RING_SIZE (16U)
#define RX_RING_MASK ((uint8_t)(RX_RING_SIZE - 1U))

#define FRAME_ERROR ((uint8_t)(1U << FE0))

// The ring. The handler writes the bytes and g_rx_head; midi_input_read()
// reads the bytes and writes g_rx_tail. Each index is one byte, so a read
// is a single instruction and needs no interrupt masking.
static volatile uint8_t g_rx_ring[RX_RING_SIZE];
static volatile uint8_t g_rx_head = 0U; // Where the next byte received goes
static volatile uint8_t g_rx_tail = 0U; // The oldest byte not yet handed over

/**
 * @brief USART receive complete interrupt, once for each byte: keeps the
 *        byte if it came whole and there is room, and does nothing else.
 */
USART_RX_ISR_ATTR void USART_RX_ISR(void);

/* DEVIATION: MISRA 8.7 (advisory) - a false positive rather than a real
 * departure from the rule, and the same one as on the tick's handler in
 * timebase.c. This function is referenced from outside this file, but not
 * from C: the interrupt vector table in the toolchain's startup object holds
 * a jump to the name __vector_18, and the linker binds that jump to this
 * function, which it can only do for a global symbol. cppcheck analyses the
 * C source alone, never sees the vector table, and so reports the function
 * as used in one file only. */
// cppcheck-suppress misra-c2012-8.7
void USART_RX_ISR(void)
{
    // The status first: it is the status of the byte the data register
    // gives next, and reading the data moves both on
    const uint8_t status = UCSR0A;
    const uint8_t byte   = UDR0;
    const uint8_t head   = g_rx_head;
    const uint8_t tail   = g_rx_tail;
    const uint8_t next   = (uint8_t)((head + 1U) & RX_RING_MASK);

    // A byte with a frame error is dropped, and so is one with nowhere to go
    if ((0U == (status & FRAME_ERROR)) && (next != tail))
    {
        g_rx_ring[head] = byte;
        g_rx_head       = next;
    }
}

/**
 * @brief Empties the ring and turns the receive interrupt on (see
 *        midi_input.h).
 */
void midi_input_init(void)
{
    g_rx_head = 0U;
    g_rx_tail = 0U;
    UCSR0B |= (uint8_t)(1U << RXCIE0);
}

/**
 * @brief  Hands over the bytes in the ring, oldest first (see
 *         midi_input_port.h).
 * @param  p_bytes    Receives the bytes; null takes none.
 * @param  max_count  The most to hand over.
 * @return How many were handed over, 0 to max_count
 */
uint8_t midi_input_read(uint8_t *p_bytes, uint8_t max_count)
{
    uint8_t count = 0U;

    if (NULL != p_bytes)
    {
        // The head as it stands now: a byte that arrives during the loop
        // waits for the next call
        const uint8_t head = g_rx_head;
        uint8_t       tail = g_rx_tail;

        while ((tail != head) && (count < max_count))
        {
            p_bytes[count] = g_rx_ring[tail];
            tail           = (uint8_t)((tail + 1U) & RX_RING_MASK);
            count++;
        }
        g_rx_tail = tail;
    }

    return count;
}
