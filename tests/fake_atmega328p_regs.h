#ifndef ATMEGA328P_REGS_H
#define ATMEGA328P_REGS_H
/**
 * @file   fake_atmega328p_regs.h
 * @brief  Stand-in for adapters/target/atmega328p_regs.h so a target adapter
 *         can be compiled and tested on a PC. It uses the same include guard
 *         as the real header, so including this first makes the adapter's own
 *         #include of the real one a no-op. Registers become plain variables,
 *         with three small hardware models behind them:
 *           - USART:  bytes written to the data register are captured, and the
 *                     "transmit buffer empty" flag can be held off for a while
 *           - ADC:    a started conversion finishes after a set number of polls
 *           - 74HC165 chain on PD2/PD3/PD4: scripted bytes are shifted out in
 *                     response to the load and clock lines
 *
 *         Everything is static, so each test program gets its own copy. Call
 *         fake_regs_reset() at the start of each test.
 * @author BonelessPig
 *
 * @copyright Copyright (c) 2026
 *
 */
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// ---- Bit positions, as in the real header ----
#define ADEN  7 // ADC Enable bit in ADCSRA
#define ADSC  6 // ADC Start Conversion bit in ADCSRA
#define ADPS2 2 // ADC Prescaler Select Bit 2 in ADCSRA
#define ADPS1 1 // ADC Prescaler Select Bit 1 in ADCSRA
#define ADPS0 0 // ADC Prescaler Select Bit 0 in ADCSRA

#define REFS0 6 // Reference Selection Bit 0 in ADMUX

#define UDRE0  5 // USART Data Register Empty flag in UCSR0A
#define RXEN0  4 // Rx Enable bit in UCSR0B
#define TXEN0  3 // Tx Enable bit in UCSR0B
#define UCSZ01 2 // Character Size bit 1 in UCSR0C
#define UCSZ00 1 // Character Size bit 0 in UCSR0C

// ---- I/O ports ----
static uint8_t g_fake_ddrb  = 0U;
static uint8_t g_fake_ddrc  = 0U;
static uint8_t g_fake_ddrd  = 0U;
static uint8_t g_fake_portb = 0U;
static uint8_t g_fake_portc = 0U;
static uint8_t g_fake_portd = 0U;
static uint8_t g_fake_pinb  = 0U;
static uint8_t g_fake_pinc  = 0U;
static uint8_t g_fake_pind  = 0U;

// ---- 74HC165 chain model ----
#define FAKE_SHIFT_CHAIN_BYTES (8U)    // Chips in the modelled chain
#define FAKE_SHIFT_CHAIN_BITS  (FAKE_SHIFT_CHAIN_BYTES * 8U)
#define FAKE_SHIFT_LOAD_MASK   (0x04U) // PD2: SH/LD, low = load
#define FAKE_SHIFT_CLK_MASK    (0x08U) // PD3: CLK, shifts on the rising edge
#define FAKE_SHIFT_DATA_MASK   (0x10U) // PD4: QH of the chip nearest the MCU

// What the chain's parallel inputs hold; byte 0 is the chip nearest the MCU
static uint8_t  g_fake_shift_inputs[FAKE_SHIFT_CHAIN_BYTES];
static uint8_t  g_fake_shift_seen_portd = 0U; // PORTD as of the last look at it
static uint16_t g_fake_shift_position   = 0U; // Bits shifted out since the last load
static uint16_t g_fake_shift_loads      = 0U; // Completed load pulses (rising edges of SH/LD)
static uint16_t g_fake_shift_clocks     = 0U; // Rising clock edges
static uint8_t  g_fake_pind_other_bits  = 0U; // Levels on the PIND pins other than PD4

// ---- ADC ----
static uint8_t  g_fake_adcsra = 0U;
static uint8_t  g_fake_admux  = 0U;
static uint16_t g_fake_adc    = 0U;
static uint16_t g_fake_adc_busy_polls = 0U; // Reads of ADCSRA that still show a conversion running

// ---- USART0 ----
#define FAKE_UART_CAPACITY (512U) // Bytes of UART output kept; later ones are dropped

static uint8_t g_fake_ubrr0h = 0U;
static uint8_t g_fake_ubrr0l = 0U;
static uint8_t g_fake_ucsr0a = 0U;
static uint8_t g_fake_ucsr0b = 0U;
static uint8_t g_fake_ucsr0c = 0U;

static uint8_t  g_fake_uart[FAKE_UART_CAPACITY + 1U]; // +1: slot that absorbs overflow
static size_t   g_fake_uart_length     = 0U;
static uint16_t g_fake_uart_busy_polls = 0U; // Reads of UCSR0A that still show the buffer full

/**
 * @brief Brings the chain model up to date with whatever the adapter last
 *        wrote to PORTD. A register access is seen before the write it makes,
 *        so every access (and the test, before it checks the counters) calls
 *        this to account for the previous one.
 */
static inline void fake_shift_sync(void)
{
    const uint8_t previous = g_fake_shift_seen_portd;
    const uint8_t current  = g_fake_portd;
    const uint8_t rising   = (uint8_t)(current & (uint8_t)~previous);
    const bool    b_load   = (0U == (current & FAKE_SHIFT_LOAD_MASK));

    if (0U != (rising & FAKE_SHIFT_LOAD_MASK))
    {
        g_fake_shift_loads++;
    }
    if (0U != (rising & FAKE_SHIFT_CLK_MASK))
    {
        g_fake_shift_clocks++;
        if (!b_load)
        {
            g_fake_shift_position++;
        }
    }
    if (b_load)
    {
        g_fake_shift_position = 0U; // Loading is level-sensitive and overrides shifting
    }
    g_fake_shift_seen_portd = current;
}

static inline uint8_t *fake_portd(void)
{
    fake_shift_sync();
    return &g_fake_portd;
}

/**
 * @brief PIND as the adapter would read it: PD4 carries the chain's output bit
 *        (most significant bit of each byte first, then 0 once the chain is
 *        empty, as with SER tied low); the other pins are whatever the test set.
 */
static inline uint8_t *fake_pind(void)
{
    uint8_t value = (uint8_t)(g_fake_pind_other_bits & (uint8_t)~FAKE_SHIFT_DATA_MASK);

    fake_shift_sync();
    if (g_fake_shift_position < FAKE_SHIFT_CHAIN_BITS)
    {
        const uint8_t byte = g_fake_shift_inputs[g_fake_shift_position / 8U];
        const uint8_t bit  = (uint8_t)(7U - (g_fake_shift_position % 8U));

        if (0U != ((byte >> bit) & 1U))
        {
            value |= FAKE_SHIFT_DATA_MASK;
        }
    }
    g_fake_pind = value;
    return &g_fake_pind;
}

/**
 * @brief ADCSRA with the hardware's self-clearing start bit: once a conversion
 *        has been started, the bit reads as set for g_fake_adc_busy_polls more
 *        accesses and then clears.
 */
static inline uint8_t *fake_adcsra(void)
{
    if (0U != (g_fake_adcsra & (1U << ADSC)))
    {
        if (g_fake_adc_busy_polls > 0U)
        {
            g_fake_adc_busy_polls--;
        }
        else
        {
            g_fake_adcsra &= (uint8_t)~(1U << ADSC);
        }
    }
    return &g_fake_adcsra;
}

/**
 * @brief UCSR0A with the "transmit buffer empty" flag clear for the next
 *        g_fake_uart_busy_polls accesses and set after that, as the hardware
 *        flag is once the previous byte has gone.
 */
static inline uint8_t *fake_ucsr0a(void)
{
    if (g_fake_uart_busy_polls > 0U)
    {
        g_fake_uart_busy_polls--;
        g_fake_ucsr0a &= (uint8_t)~(1U << UDRE0);
    }
    else
    {
        g_fake_ucsr0a |= (uint8_t)(1U << UDRE0);
    }
    return &g_fake_ucsr0a;
}

/**
 * @brief Where the next byte written to the UART data register lands.
 */
static inline uint8_t *fake_udr0(void)
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
static inline void fake_uart_clear(void)
{
    g_fake_uart_length = 0U;
}

/**
 * @brief Puts every fake register and model back to its power-on state.
 */
static inline void fake_regs_reset(void)
{
    g_fake_ddrb  = 0U;
    g_fake_ddrc  = 0U;
    g_fake_ddrd  = 0U;
    g_fake_portb = 0U;
    g_fake_portc = 0U;
    g_fake_portd = 0U;
    g_fake_pinb  = 0U;
    g_fake_pinc  = 0U;
    g_fake_pind  = 0U;

    for (size_t i = 0U; i < FAKE_SHIFT_CHAIN_BYTES; i++)
    {
        g_fake_shift_inputs[i] = 0U;
    }
    g_fake_shift_seen_portd = 0U;
    g_fake_shift_position   = 0U;
    g_fake_shift_loads      = 0U;
    g_fake_shift_clocks     = 0U;
    g_fake_pind_other_bits  = 0U;

    g_fake_adcsra = 0U;
    g_fake_admux  = 0U;
    g_fake_adc    = 0U;
    g_fake_adc_busy_polls = 0U;

    g_fake_ubrr0h = 0U;
    g_fake_ubrr0l = 0U;
    g_fake_ucsr0a = 0U;
    g_fake_ucsr0b = 0U;
    g_fake_ucsr0c = 0U;
    g_fake_uart_busy_polls = 0U;
    fake_uart_clear();
}

#define DDRB  g_fake_ddrb
#define DDRC  g_fake_ddrc
#define DDRD  g_fake_ddrd
#define PORTB g_fake_portb
#define PORTC g_fake_portc
#define PORTD (*fake_portd())
#define PINB  g_fake_pinb
#define PINC  g_fake_pinc
#define PIND  (*fake_pind())

#define ADCSRA (*fake_adcsra())
#define ADMUX  g_fake_admux
#define ADC    g_fake_adc

#define UBRR0H g_fake_ubrr0h
#define UBRR0L g_fake_ubrr0l
#define UCSR0A (*fake_ucsr0a())
#define UCSR0B g_fake_ucsr0b
#define UCSR0C g_fake_ucsr0c
#define UDR0   (*fake_udr0())

#endif /* ATMEGA328P_REGS_H */
