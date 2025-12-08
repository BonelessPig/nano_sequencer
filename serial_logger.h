#ifndef SERIAL_LOGGER_H 
#define SERIAL_LOGGER_H

#include <stdio.h>
#include <avr/io.h>

#define F_CPU 16000000UL // ATmega328P's clock frequency
#define BAUD 9600        // Desired baud rate

#include <util/setbaud.h>


void add_char_serial(char c);

void log_serial(const char *s);
void serial_init(void);

#endif