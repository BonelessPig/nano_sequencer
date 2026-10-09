/**
 * @file analog_reader.c
 * @author BonelessPig
 * @brief reads the value of an analog input pin using the ADC
 * @version 0.1
 * @date 2026-02-18
 * 
 * @copyright Copyright (c) 2026
 * 
 */
#include "analog_reader.h"
#include "../mcu/atmega328p_regs.h"
#include "../common/common_types.h"

// Upper bound on polls of the conversion-complete flag. A conversion takes at
// most ~200 us (25 ADC clocks at 125 kHz); this many polls takes a few
// milliseconds at 16 MHz, so it only trips if the ADC is genuinely stuck.
#define ADC_TIMEOUT_LOOPS 10000

/**
 * @brief Reads the value of an analog input pin and stores it in the provided variable.
 * @param value pointer to a short variable where the Analog to Digital Converter (ADC) 
 *              value will be stored
 * @param channel the ADC channel to read from (0-7 for ATmega328P)
 * @return int status code (0 for success)
 */
int read_analog_value(short* value, unsigned char channel) {

    if (value == 0) return ERR_INVALID_PARAM; // Nowhere to store the result
    if (channel > 7) return ERR_INVALID_PARAM; // Invalid channel
    ADMUX = (1 << REFS0) | (channel & 0x0F); // Set reference to AVcc and select ADC channel (0-7)
    ADCSRA |= (1 << ADSC); // Start ADC conversion

    unsigned int loops_left = ADC_TIMEOUT_LOOPS;
    while (ADCSRA & (1 << ADSC)) { // Wait for conversion to complete
        if (--loops_left == 0) return ERR_TIMEOUT; // ADC never finished (e.g. not enabled)
    }
    *value = ADC; // Read the ADC value (10-bit result from ADC register) 0-1023 for 0-5V input
    return STATUS_OK; // Return success
}