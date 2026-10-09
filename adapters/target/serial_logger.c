/**
 * @file   serial_logger.c
 * @brief  Target implementation of the log port: formatted logging over USART0.
 * @author BonelessPig
 * @date   2025-12-08
 *
 * @copyright Copyright (c) 2025
 *
 */
#include "serial_logger.h"
#include "log_port.h"
#include "port_status.h"
#include "utilities.h"
#include "atmega328p_regs.h"

#define BAUD 9600 // Desired baud rate

// Calculated per the ATmega328P datasheet, in place of <util/setbaud.h>
#define UUBR_VALUE  (((F_CPU) / (16UL * BAUD)) - 1) // USART Baud Rate Register value
#define UBRRH_VALUE ((unsigned char)(UUBR_VALUE >> 8)) // High byte of UBRR value
#define UBRRL_VALUE ((unsigned char)UUBR_VALUE)        // Low  byte of UBRR value

static LogLevel currentLogLevel = LOGLVL_OFF; // Default log level


/**
 * @brief Initializes the serial logger with the specified log level.
 * @param level level to set for logging
 * @return port_status_t status code (STATUS_OK for success)
 */
port_status_t serial_init(LogLevel level) {
    UBRR0H = UBRRH_VALUE; // Set baud rate high byte
    UBRR0L = UBRRL_VALUE; // Set baud rate low byte

    // Set these explicitly rather than relying on reset defaults, in case a
    // bootloader left them changed (e.g. double-speed mode enabled)
    UCSR0A = 0; // Normal speed (U2X0 = 0), no multi-processor mode
    UCSR0C = (1 << UCSZ01) | (1 << UCSZ00); // Frame format: 8 data bits, no parity, 1 stop bit

    UCSR0B = (1 << RXEN0) | (1 << TXEN0); // Enable receiver and transmitter

    currentLogLevel = level; // Sets the Log Level
    return STATUS_OK; // Return success
}



/**
 * @brief Adds a character to the serial output.
 * @param c character to add
 */
void add_char_serial(char c) {
    while (!(UCSR0A & (1 << UDRE0))); // Wait for empty transmit buffer
    UDR0 = c; // Put data into buffer, sends the data
}



/**
 * @brief Logs a string to the serial output if the log level is appropriate.
 * @param level LogLevel of the message
 * @param format format string (like printf)
 * @param ... additional arguments for the format string
 */
void log_serial(LogLevel level, const char *format, ...) {

    if (level == LOGLVL_OFF || level > currentLogLevel) return; // Skip logging if level is more verbose than the current setting

    static char buffer[64]; // Buffer for formatted output

    va_list args;
    va_start(args, format);
    int len = vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    const char* s = buffer;
    while (*s) add_char_serial(*s++);

    if ((unsigned int)len >= sizeof(buffer)) {
        // If the message was truncated, indicate this in the output
        add_char_serial('\r');
        add_char_serial('\n');
        add_char_serial('[');
        add_char_serial('T');
        add_char_serial('R');
        add_char_serial('U');
        add_char_serial('N');
        add_char_serial('C');
        add_char_serial('A');
        add_char_serial('T');
        add_char_serial('E');
        add_char_serial('D');
        add_char_serial(']');
        add_char_serial('\r');
        add_char_serial('\n');
    }
}



/**
 * @brief Logs one step's note value at DEBUG level (see log_port.h).
 * @param step step index
 * @param note note value for that step
 */
void log_step_note(uint8_t step, uint8_t note) {
    log_serial(LOGLVL_DEBUG, "Step %d Note = %d\r\n", step, note); // Print note value to serial
}



/**
 * @brief Logs a failed operation and its status code at ERROR level (see log_port.h).
 * @param what which operation failed
 * @param status the status code it returned
 */
void log_error(log_error_id_t what, port_status_t status) {
    switch (what) {
        case LOG_ERROR_INIT:
            log_serial(LOGLVL_ERROR, "Initialization failed, status code = %d\r\n", status);
            break;
        case LOG_ERROR_STEP_READ:
            log_serial(LOGLVL_ERROR, "Step note read failed, status code = %d\r\n", status);
            break;
        case LOG_ERROR_TEMPO_READ:
            log_serial(LOGLVL_ERROR, "ADC read failed for delay channel, status code = %d\r\n", status);
            break;
        default:
            break; // Unknown id: nothing sensible to print
    }
}
