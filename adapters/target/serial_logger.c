/**
 * @file   serial_logger.c
 * @brief  Target implementation of the log port for the debug build: text
 *         logging over USART0. The release build links null_logger.c instead.
 *         Nothing here waits for the UART. A message is written piece by
 *         piece (fixed text, then numbers as decimal digits) into a queue,
 *         and log_poll() sends one queued byte each time the transmitter has
 *         room. A message that does not fit in the queue is dropped whole.
 * @author BonelessPig
 * @date   2025-12-08
 *
 * @copyright Copyright (c) 2025
 *
 */
#include "logger.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "log_port.h"
#include "port_status.h"
#include "atmega328p_usart_regs.h"

// F_CPU is provided by the build (see Makefile / -DF_CPU) rather than defined
// here, so there is a single source of truth for the clock speed.
#ifndef F_CPU
#error "F_CPU must be defined by the build (e.g. -DF_CPU=16000000UL)"
#endif

#define BAUD (115200UL) // Desired baud rate

// Calculated per the ATmega328P datasheet, in place of <util/setbaud.h>.
// Double-speed mode is used because its finer divisor steps get much closer to
// 115200 at 16 MHz (2.1 % fast) than normal speed can (8.5 % fast). The
// division rounds to the nearest divisor rather than down.
#define UBRR_SAMPLES_PER_BIT (8UL) // Double-speed mode samples each bit 8 times
#define UBRR_CLOCKS_PER_BIT  (UBRR_SAMPLES_PER_BIT * BAUD) // CPU clocks per bit at a divisor of 1
#define UBRR_VALUE  ((((F_CPU) + (UBRR_CLOCKS_PER_BIT / 2UL)) / UBRR_CLOCKS_PER_BIT) - 1UL) // USART Baud Rate Register value
#define BYTE_RANGE  (256UL) // Number of values one byte can hold
#define UBRRH_VALUE ((uint8_t)(UBRR_VALUE / BYTE_RANGE)) // High byte of UBRR value
#define UBRRL_VALUE ((uint8_t)(UBRR_VALUE % BYTE_RANGE)) // Low  byte of UBRR value

#define DECIMAL_BASE       (10U)
#define MAX_DECIMAL_DIGITS (5U) // Digits in the largest 16-bit value (65535)

// Room for the two longest messages one after the other (53 characters each)
#define TX_QUEUE_SIZE (128U)

static log_level_t g_log_level = LOGLVL_OFF; // Default log level

static char    g_tx_queue[TX_QUEUE_SIZE]; // Bytes waiting to be sent, as a ring
static uint8_t g_tx_tail  = 0U;           // Index of the next byte to send
static uint8_t g_tx_count = 0U;           // Bytes waiting, 0 to TX_QUEUE_SIZE

static uint8_t g_message_start_count = 0U;  // g_tx_count when the message began
static bool    g_b_message_dropped   = false; // true once a byte of it did not fit



/**
 * @brief Tells whether a message of the given level should be sent.
 * @param level level of the message
 * @return true if the level is at or below the configured verbosity
 */
static bool is_level_enabled(log_level_t level)
{
    return (LOGLVL_OFF != level) && (level <= g_log_level);
}



/**
 * @brief Starts a message. Everything written until end_message() is kept or
 *        dropped together.
 */
static void begin_message(void)
{
    g_message_start_count = g_tx_count;
    g_b_message_dropped   = false;
}



/**
 * @brief Ends a message. If any of it did not fit in the queue, the part that
 *        did is taken back out, so only whole messages are ever sent.
 */
static void end_message(void)
{
    if (g_b_message_dropped)
    {
        g_tx_count = g_message_start_count;
    }
}



/**
 * @brief Adds one character to the queue, or marks the message as dropped if
 *        the queue is full.
 * @param character character to queue
 */
static void write_char(char character)
{
    if (g_tx_count < TX_QUEUE_SIZE)
    {
        const uint8_t index = (uint8_t)((g_tx_tail + g_tx_count) % TX_QUEUE_SIZE);

        g_tx_queue[index] = character;
        g_tx_count++;
    }
    else
    {
        g_b_message_dropped = true;
    }
}



/**
 * @brief Queues a null-terminated string.
 * @param p_text string to queue; must not be null
 */
static void write_text(const char *p_text)
{
    for (uint8_t i = 0U; '\0' != p_text[i]; i++)
    {
        write_char(p_text[i]);
    }
}



/**
 * @brief Queues a number as decimal digits, with no padding.
 * @param value number to queue, 0 to 65535
 */
static void write_decimal(uint16_t value)
{
    char     digits[MAX_DECIMAL_DIGITS];
    uint8_t  count     = 0U;
    uint16_t remaining = value;

    // Digits come out least significant first, so collect them and queue in reverse
    do
    {
        const uint8_t digit = (uint8_t)(remaining % DECIMAL_BASE);

        digits[count] = (char)('0' + digit);
        count++;
        remaining /= DECIMAL_BASE;
    } while (remaining > 0U);

    while (count > 0U)
    {
        count--;
        write_char(digits[count]);
    }
}



/**
 * @brief Sets up USART0, empties the queue and sets the log level (see logger.h).
 * @param level level to set for logging
 * @return port_status_t status code (STATUS_OK for success)
 */
port_status_t logger_init(log_level_t level)
{
    UBRR0H = UBRRH_VALUE; // Set baud rate high byte
    UBRR0L = UBRRL_VALUE; // Set baud rate low byte

    // Set these explicitly rather than relying on reset defaults, in case a
    // bootloader left them changed
    UCSR0A = (uint8_t)(1U << U2X0); // Double speed, no multi-processor mode
    UCSR0C = (uint8_t)((1U << UCSZ01) | (1U << UCSZ00)); // Frame format: 8 data bits, no parity, 1 stop bit

    UCSR0B = (uint8_t)((1U << RXEN0) | (1U << TXEN0)); // Enable receiver and transmitter

    g_tx_tail  = 0U; // Empty the queue
    g_tx_count = 0U;

    g_log_level = level; // Sets the Log Level
    return STATUS_OK; // Return success
}



/**
 * @brief Logs one step's note value at DEBUG level (see log_port.h).
 * @param step step index
 * @param note note value for that step
 */
void log_step_note(uint8_t step, uint8_t note)
{
    if (is_level_enabled(LOGLVL_DEBUG))
    {
        begin_message();
        write_text("Step ");
        write_decimal(step);
        write_text(" Note = ");
        write_decimal(note);
        write_text("\r\n");
        end_message();
    }
}



/**
 * @brief Logs a failed operation and its status code at ERROR level (see log_port.h).
 * @param what which operation failed
 * @param status the status code it returned
 */
void log_error(log_error_id_t what, port_status_t status)
{
    const char *p_text = NULL;

    switch (what)
    {
        case LOG_ERROR_INIT:
            p_text = "Initialization failed, status code = ";
            break;

        case LOG_ERROR_STEP_READ:
            p_text = "Step note read failed, status code = ";
            break;

        case LOG_ERROR_TEMPO_READ:
            p_text = "ADC read failed for delay channel, status code = ";
            break;

        default:
            // Unknown id: nothing sensible to print
            break;
    }

    if ((NULL != p_text) && is_level_enabled(LOGLVL_ERROR))
    {
        begin_message();
        write_text(p_text);
        write_decimal((uint16_t)status);
        write_text("\r\n");
        end_message();
    }
}



/**
 * @brief Sends the next queued byte if there is one and the transmitter has
 *        room for it (see log_port.h). Never waits.
 */
void log_poll(void)
{
    if (g_tx_count > 0U)
    {
        if (0U != (UCSR0A & (1U << UDRE0))) // Transmit buffer empty
        {
            UDR0 = (uint8_t)g_tx_queue[g_tx_tail]; // Put data into buffer, sends the data
            g_tx_tail = (uint8_t)((g_tx_tail + 1U) % TX_QUEUE_SIZE);
            g_tx_count--;
        }
    }
}



/**
 * @brief Sends everything still queued, waiting for the transmitter between
 *        bytes (see log_port.h).
 */
void log_flush(void)
{
    while (g_tx_count > 0U)
    {
        log_poll();
    }
}
