/**
 * @file   serial_logger.c
 * @brief  Target implementation of the log port for the debug build: text
 *         logging over USART0. The release build links null_logger.c instead.
 *         Messages are sent piece by piece (fixed text, then numbers as
 *         decimal digits), so no formatting buffer is needed.
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

static log_level_t g_log_level = LOGLVL_OFF; // Default log level



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
 * @brief Sends one character over the serial port, waiting for room first.
 * @param character character to send
 */
static void write_char(char character)
{
    while (0U == (UCSR0A & (1U << UDRE0))) // Wait for empty transmit buffer
    {
    }
    UDR0 = (uint8_t)character; // Put data into buffer, sends the data
}



/**
 * @brief Sends a null-terminated string over the serial port.
 * @param p_text string to send; must not be null
 */
static void write_text(const char *p_text)
{
    for (uint8_t i = 0U; '\0' != p_text[i]; i++)
    {
        write_char(p_text[i]);
    }
}



/**
 * @brief Sends a number over the serial port as decimal digits, with no padding.
 * @param value number to send, 0 to 65535
 */
static void write_decimal(uint16_t value)
{
    char     digits[MAX_DECIMAL_DIGITS];
    uint8_t  count     = 0U;
    uint16_t remaining = value;

    // Digits come out least significant first, so collect them and send in reverse
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
 * @brief Sets up USART0 and the log level (see logger.h).
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
        write_text("Step ");
        write_decimal(step);
        write_text(" Note = ");
        write_decimal(note);
        write_text("\r\n");
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
        write_text(p_text);
        write_decimal((uint16_t)status);
        write_text("\r\n");
    }
}
