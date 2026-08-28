#ifndef __SERIAL_LOGGER_H__
#define __SERIAL_LOGGER_H__
/**
 * @file   serial_logger.h
 * @brief  Header file for serial logging functionality on AVR microcontrollers.
 * @author BonelessPig
 * @date   2025-12-08
 * 
 * @copyright Copyright (c) 2025
 * 
 */


// These definitions are provided below to avoid dependency on <util/setbaud.h> 
// per the atmega328p datasheet

#define F_CPU 16000000UL // 16 MHz clock speed
#define BAUD 9600 // Desired baud rate        

#define UUBR_Value (((F_CPU) / (16UL * BAUD)) -1 ) // Calculate UBRR(USART Baud Rate Register) value

#define UBRRH_VALUE ((unsigned char)(UUBR_Value >> 8)) // High byte of UBRR value
#define UBRRL_VALUE ((unsigned char)UUBR_Value)        // Low  byte of UBRR value

#define UBRR0H (*(volatile char*)0xC5) // USART Baud Rate Register High Byte
#define UBRR0L (*(volatile char*)0xC4) // USART Baud Rate Register Low  Byte

#define UCSR0A (*(volatile char*)0xC0) // USART Control and Status Register A
#define UCSR0B (*(volatile char*)0xC1) // USART Control and Status Register B

#define UDRE0  5 // USART Data Register Empty flag in UCSR0A

#define RXEN0  4 // Rx Enable bit in UCSR0B
#define TXEN0  3 // Tx Enable bit in UCSR0B

#define UDR0   (*(volatile char*)0xC6) // USART I/O Data Register

#define va_start(v, l) __builtin_va_start(v, l)
#define va_end(v)      __builtin_va_end(v)

typedef __builtin_va_list va_list;


//int vsnprintf(char *str, long size, const char *format, va_list ap);



/**
 * @brief Log levels for serial logging
 */
typedef enum {
    LOGLVL_OFF = 0,    // No logging
    LOGLVL_FATAL,      // Critical errors that cause application termination
    LOGLVL_ERROR,      // Errors that prevent normal operation
    LOGLVL_WARN,       // Warnings, potential issues
    LOGLVL_INFO,       // General information about application flow
    LOGLVL_DEBUG,      // Detailed information for debugging
    LOGLVL_TRACE       // Very detailed, fine-grained information
} LogLevel;

/**
 * @brief Adds a character to the serial output.
 * @param c character to add
 */
void add_char_serial(char c);

/**
 * @brief Logs a string to the serial output if the log level is appropriate.
 * @param level LogLevel of the message
 * @param format format string (like printf)
 * @param ... additional arguments for the format string
 */
void log_serial(LogLevel level, const char *s, ...);

/**
 * @brief Initializes the serial logger with the specified log level.
 * @param level level to set for logging
 * @return int status code (0 for success)
 */
int serial_init(LogLevel level);

#endif