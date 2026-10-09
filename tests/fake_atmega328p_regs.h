#ifndef ATMEGA328P_REGS_H
#define ATMEGA328P_REGS_H
/**
 * @file   fake_atmega328p_regs.h
 * @brief  Stand-in for adapters/target/atmega328p_regs.h so a target adapter
 *         can be compiled and tested on a PC. It uses the same include guard
 *         as the real header, so including this first makes the adapter's own
 *         #include of the real one a no-op. Registers become plain variables,
 *         and bytes written to the UART data register are captured.
 *
 *         Only the USART registers are faked; add others as tests need them.
 * @author BonelessPig
 *
 * @copyright Copyright (c) 2026
 *
 */
#include <stddef.h>
#include <stdint.h>

#define FAKE_UART_CAPACITY (512U) // Bytes of UART output kept; later ones are dropped

static uint8_t g_fake_ubrr0h = 0U;
static uint8_t g_fake_ubrr0l = 0U;
static uint8_t g_fake_ucsr0a = 0U;
static uint8_t g_fake_ucsr0b = 0U;
static uint8_t g_fake_ucsr0c = 0U;

static uint8_t g_fake_uart[FAKE_UART_CAPACITY + 1U]; // +1: slot that absorbs overflow
static size_t  g_fake_uart_length = 0U;

#define UDRE0  5 // USART Data Register Empty flag in UCSR0A
#define RXEN0  4 // Rx Enable bit in UCSR0B
#define TXEN0  3 // Tx Enable bit in UCSR0B
#define UCSZ01 2 // Character Size bit 1 in UCSR0C
#define UCSZ00 1 // Character Size bit 0 in UCSR0C

/**
 * @brief The status register always reads as "transmit buffer empty", as the
 *        hardware flag would once the previous byte has gone, so the adapter's
 *        wait loop never blocks.
 */
static uint8_t *fake_ucsr0a(void)
{
    g_fake_ucsr0a |= (uint8_t)(1U << UDRE0);
    return &g_fake_ucsr0a;
}

/**
 * @brief Where the next byte written to the UART data register lands.
 */
static uint8_t *fake_udr0(void)
{
    uint8_t *p_slot = &g_fake_uart[FAKE_UART_CAPACITY];

    if (g_fake_uart_length < FAKE_UART_CAPACITY)
    {
        p_slot = &g_fake_uart[g_fake_uart_length];
        g_fake_uart_length++;
    }
    return p_slot;
}

/**
 * @brief Forgets everything captured from the UART so far.
 */
static void fake_uart_clear(void)
{
    g_fake_uart_length = 0U;
}

#define UBRR0H g_fake_ubrr0h
#define UBRR0L g_fake_ubrr0l
#define UCSR0A (*fake_ucsr0a())
#define UCSR0B g_fake_ucsr0b
#define UCSR0C g_fake_ucsr0c
#define UDR0   (*fake_udr0())

#endif /* ATMEGA328P_REGS_H */
