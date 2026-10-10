#ifndef ATMEGA328P_REGS_H
#define ATMEGA328P_REGS_H
#define ATMEGA328P_USART_REGS_H  // This file stands in for the USART header too
#define ATMEGA328P_TIMER2_REGS_H // And for the Timer/Counter2 header
#define ATMEGA328P_SPI_REGS_H    // And for the SPI header
/**
 * @file   fake_atmega328p_regs.h
 * @brief  Stand-in for adapters/target/atmega328p_regs.h,
 *         atmega328p_usart_regs.h, atmega328p_timer2_regs.h and
 *         atmega328p_spi_regs.h so a target adapter can be compiled and
 *         tested on a PC. It claims the real headers' include guards, so
 *         including this first makes the adapter's own #include of a real
 *         one a no-op. Registers become plain variables,
 *         with four small hardware models behind them:
 *           - USART:  bytes written to the data register are captured, and the
 *                     "transmit buffer empty" flag can be held off for a while
 *           - ADC:    a started conversion finishes after a set number of polls
 *           - SPI:    a byte written to the data register is captured and a
 *                     scripted one comes back after a set number of polls,
 *                     but only if the SPI is enabled as master
 *           - 74HC165 chain with its load line on PB1: scripted bytes are
 *                     handed out eight clocks at a time, as an SPI transfer
 *                     clocks them, in response to the load line
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
#define U2X0   1 // Double Transmission Speed bit in UCSR0A
#define RXEN0  4 // Rx Enable bit in UCSR0B
#define TXEN0  3 // Tx Enable bit in UCSR0B
#define UCSZ01 2 // Character Size bit 1 in UCSR0C
#define UCSZ00 1 // Character Size bit 0 in UCSR0C

#define SREG_I 7 // Global Interrupt Enable bit in SREG

#define WGM21  1 // Waveform Generation Mode bit 1 in TCCR2A
#define CS22   2 // Clock Select bit 2 in TCCR2B
#define OCIE2A 1 // Output Compare Match A Interrupt Enable bit in TIMSK2

#define SPE  6 // SPI Enable bit in SPCR
#define MSTR 4 // Master/Slave Select bit in SPCR
#define SPR0 0 // SPI Clock Rate Select bit 0 in SPCR
#define SPIF 7 // SPI Interrupt Flag in SPSR

// The interrupt handler becomes an ordinary function the test can call
#define TIMER2_COMPA_ISR fake_timer2_compa_isr
#define TIMER2_COMPA_ISR_ATTR

// ---- CPU and Timer/Counter2 ----
static uint8_t g_fake_sreg   = 0U;
static uint8_t g_fake_tccr2a = 0U;
static uint8_t g_fake_tccr2b = 0U;
static uint8_t g_fake_tcnt2  = 0U;
static uint8_t g_fake_ocr2a  = 0U;
static uint8_t g_fake_timsk2 = 0U;

// What the timer registers held each time SREG was accessed, so a test can
// see whether the timer was fully set up before interrupts were enabled
static uint8_t g_fake_tccr2b_at_sreg = 0U;
static uint8_t g_fake_timsk2_at_sreg = 0U;
static uint8_t g_fake_ocr2a_at_sreg  = 0U;

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

// What PORTB held the first time DDRB was accessed after a reset, so a test
// can see whether a pin was given its level before it was made an output
static uint8_t g_fake_portb_at_first_ddrb = 0U;
static bool    g_fake_b_ddrb_accessed     = false;

// The same for port D
static uint8_t g_fake_portd_at_first_ddrd = 0U;
static bool    g_fake_b_ddrd_accessed     = false;

// ---- 74HC165 chain model ----
#define FAKE_SHIFT_CHAIN_BYTES (8U)    // Chips in the modelled chain
#define FAKE_SHIFT_CHAIN_BITS  (FAKE_SHIFT_CHAIN_BYTES * 8U)
#define FAKE_SHIFT_LOAD_MASK   (0x02U) // PB1: SH/LD, low = load

// What the chain's parallel inputs hold; byte 0 is the chip nearest the MCU
static uint8_t  g_fake_shift_inputs[FAKE_SHIFT_CHAIN_BYTES];
static uint8_t  g_fake_shift_seen_portb = 0U; // PORTB as of the last look at it
static uint16_t g_fake_shift_position   = 0U; // Bits shifted out since the last load
static uint16_t g_fake_shift_loads      = 0U; // Completed load pulses (rising edges of SH/LD)
static uint16_t g_fake_shift_clocks     = 0U; // Rising clock edges

// ---- SPI ----
#define FAKE_SPI_CAPACITY    (16U)   // Transfers scripted and recorded; later ones read 0
#define FAKE_SPI_MASTER_MASK (0x50U) // SPE and MSTR: what SPCR needs for a transfer to run

static uint8_t  g_fake_spcr = 0U;
static uint8_t  g_fake_spsr = 0U;
static uint8_t  g_fake_spdr = 0U;
static uint8_t  g_fake_spi_rx[FAKE_SPI_CAPACITY]; // What each transfer reads, in order
static uint8_t  g_fake_spi_tx[FAKE_SPI_CAPACITY]; // What each transfer sent, in order
static uint16_t g_fake_spi_transfers  = 0U;       // Transfers completed
static uint16_t g_fake_spi_busy_polls = 0U;       // Reads of SPSR before a transfer completes
static uint16_t g_fake_spi_polls_left = 0U;       // The same, for the transfer in progress
static bool     g_fake_spi_b_pending  = false;    // A byte has been written and not yet clocked

// What port B looked like each time SPCR was accessed, so a test can see
// whether SS was a high output before master mode was selected
static uint8_t g_fake_ddrb_at_spcr  = 0U;
static uint8_t g_fake_portb_at_spcr = 0U;

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
 *        wrote to PORTB. A register access is seen before the write it makes,
 *        so every access (and the test, before it checks the counters) calls
 *        this to account for the previous one.
 */
static inline void fake_shift_sync(void)
{
    const uint8_t previous = g_fake_shift_seen_portb;
    const uint8_t current  = g_fake_portb;
    const uint8_t rising   = (uint8_t)(current & (uint8_t)~previous);

    if (0U != (rising & FAKE_SHIFT_LOAD_MASK))
    {
        g_fake_shift_loads++;
    }
    if (0U == (current & FAKE_SHIFT_LOAD_MASK))
    {
        g_fake_shift_position = 0U; // Loading is level-sensitive and overrides shifting
    }
    g_fake_shift_seen_portb = current;
}

static inline uint8_t *fake_portb(void)
{
    fake_shift_sync();
    return &g_fake_portb;
}

/**
 * @brief DDRB, noting what PORTB held the first time it is touched.
 */
static inline uint8_t *fake_ddrb(void)
{
    if (!g_fake_b_ddrb_accessed)
    {
        g_fake_portb_at_first_ddrb = g_fake_portb;
        g_fake_b_ddrb_accessed     = true;
    }
    return &g_fake_ddrb;
}

/**
 * @brief DDRD, noting what PORTD held the first time it is touched.
 */
static inline uint8_t *fake_ddrd(void)
{
    if (!g_fake_b_ddrd_accessed)
    {
        g_fake_portd_at_first_ddrd = g_fake_portd;
        g_fake_b_ddrd_accessed     = true;
    }
    return &g_fake_ddrd;
}

/**
 * @brief The chain's output bit as it stands: the most significant bit of
 *        each byte first, then 0 once the chain is empty, as with SER tied low.
 */
static inline uint8_t fake_shift_output_bit(void)
{
    uint8_t bit = 0U;

    if (g_fake_shift_position < FAKE_SHIFT_CHAIN_BITS)
    {
        const uint8_t byte  = g_fake_shift_inputs[g_fake_shift_position / 8U];
        const uint8_t shift = (uint8_t)(7U - (g_fake_shift_position % 8U));

        bit = (uint8_t)((byte >> shift) & 1U);
    }
    return bit;
}

/**
 * @brief Eight clocks of the chain, as one SPI transfer gives it: each bit is
 *        sampled and then the clock's rising edge moves the chain on. While
 *        the load line is low the chain does not shift, so the first bit is
 *        read eight times.
 * @return The eight bits read, first bit in bit 7.
 */
static inline uint8_t fake_shift_clock_byte(void)
{
    uint8_t byte = 0U;

    fake_shift_sync();
    for (uint8_t i = 0U; i < 8U; i++)
    {
        byte = (uint8_t)((uint8_t)(byte << 1U) | fake_shift_output_bit());
        g_fake_shift_clocks++;
        if (0U != (g_fake_portb & FAKE_SHIFT_LOAD_MASK))
        {
            g_fake_shift_position++;
        }
    }
    return byte;
}

/**
 * @brief SPCR, noting how port B stood at the moment it is touched.
 */
static inline uint8_t *fake_spcr(void)
{
    g_fake_ddrb_at_spcr  = g_fake_ddrb;
    g_fake_portb_at_spcr = g_fake_portb;
    return &g_fake_spcr;
}

/**
 * @brief SPSR with the hardware's transfer-complete flag: once a byte has
 *        been written to the data register, the flag reads as clear for
 *        g_fake_spi_busy_polls accesses and then sets, with the scripted
 *        byte in the data register. The transfer only runs if the SPI is
 *        enabled as master.
 */
static inline uint8_t *fake_spsr(void)
{
    const bool b_master = (FAKE_SPI_MASTER_MASK == (g_fake_spcr & FAKE_SPI_MASTER_MASK));

    if (g_fake_spi_b_pending && b_master)
    {
        if (g_fake_spi_polls_left > 0U)
        {
            g_fake_spi_polls_left--;
        }
        else
        {
            if (g_fake_spi_transfers < FAKE_SPI_CAPACITY)
            {
                g_fake_spi_tx[g_fake_spi_transfers] = g_fake_spdr;
                g_fake_spdr = g_fake_spi_rx[g_fake_spi_transfers];
            }
            else
            {
                g_fake_spdr = 0U;
            }
            g_fake_spi_transfers++;
            g_fake_spi_b_pending = false;
            g_fake_spsr |= (uint8_t)(1U << SPIF);
        }
    }
    return &g_fake_spsr;
}

/**
 * @brief SPDR. With the transfer-complete flag set, the access is the read
 *        of the byte that came in, and clears the flag as the hardware does.
 *        Otherwise it is the write that starts a transfer.
 */
static inline uint8_t *fake_spdr(void)
{
    if (0U != (g_fake_spsr & (1U << SPIF)))
    {
        g_fake_spsr &= (uint8_t)~(1U << SPIF);
    }
    else
    {
        g_fake_spi_b_pending  = true;
        g_fake_spi_polls_left = g_fake_spi_busy_polls;
    }
    return &g_fake_spdr;
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
 * @brief SREG, noting the state of the timer at the moment it is touched.
 */
static inline uint8_t *fake_sreg(void)
{
    g_fake_tccr2b_at_sreg = g_fake_tccr2b;
    g_fake_timsk2_at_sreg = g_fake_timsk2;
    g_fake_ocr2a_at_sreg  = g_fake_ocr2a;
    return &g_fake_sreg;
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
    g_fake_portb_at_first_ddrb = 0U;
    g_fake_b_ddrb_accessed     = false;
    g_fake_portd_at_first_ddrd = 0U;
    g_fake_b_ddrd_accessed     = false;

    for (size_t i = 0U; i < FAKE_SHIFT_CHAIN_BYTES; i++)
    {
        g_fake_shift_inputs[i] = 0U;
    }
    g_fake_shift_seen_portb = 0U;
    g_fake_shift_position   = 0U;
    g_fake_shift_loads      = 0U;
    g_fake_shift_clocks     = 0U;

    g_fake_spcr = 0U;
    g_fake_spsr = 0U;
    g_fake_spdr = 0U;
    for (size_t i = 0U; i < FAKE_SPI_CAPACITY; i++)
    {
        g_fake_spi_rx[i] = 0U;
        g_fake_spi_tx[i] = 0U;
    }
    g_fake_spi_transfers  = 0U;
    g_fake_spi_busy_polls = 0U;
    g_fake_spi_polls_left = 0U;
    g_fake_spi_b_pending  = false;
    g_fake_ddrb_at_spcr   = 0U;
    g_fake_portb_at_spcr  = 0U;

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

    g_fake_sreg   = 0U;
    g_fake_tccr2a = 0U;
    g_fake_tccr2b = 0U;
    g_fake_tcnt2  = 0U;
    g_fake_ocr2a  = 0U;
    g_fake_timsk2 = 0U;
    g_fake_tccr2b_at_sreg = 0U;
    g_fake_timsk2_at_sreg = 0U;
    g_fake_ocr2a_at_sreg  = 0U;
}

#define DDRB  (*fake_ddrb())
#define DDRC  g_fake_ddrc
#define DDRD  (*fake_ddrd())
#define PORTB (*fake_portb())
#define PORTC g_fake_portc
#define PORTD g_fake_portd
#define PINB  g_fake_pinb
#define PINC  g_fake_pinc
#define PIND  g_fake_pind

#define SPCR (*fake_spcr())
#define SPSR (*fake_spsr())
#define SPDR (*fake_spdr())

#define ADCSRA (*fake_adcsra())
#define ADMUX  g_fake_admux
#define ADC    g_fake_adc

#define UBRR0H g_fake_ubrr0h
#define UBRR0L g_fake_ubrr0l
#define UCSR0A (*fake_ucsr0a())
#define UCSR0B g_fake_ucsr0b
#define UCSR0C g_fake_ucsr0c
#define UDR0   (*fake_udr0())

#define SREG   (*fake_sreg())
#define TCCR2A g_fake_tccr2a
#define TCCR2B g_fake_tccr2b
#define TCNT2  g_fake_tcnt2
#define OCR2A  g_fake_ocr2a
#define TIMSK2 g_fake_timsk2

#endif /* ATMEGA328P_REGS_H */
