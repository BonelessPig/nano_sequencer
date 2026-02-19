/**
 * @file analog_reader.c
 * @author BonelessPig
 * @brief reads the value of an analog input pin and prints it to serial
 * @version 0.1
 * @date 2026-02-18
 * 
 * @copyright Copyright (c) 2026
 * 
 */
#include "analog_reader.h"
#include "register_init.h"

/**
 * @brief Reads the value of an analog input pin and stores it in the provided variable.
 * @param value pointer to a short variable where the ADC value will be stored
 * @param channel the ADC channel to read from (0-7 for ATmega328P)
 */
void read_analog_value(short* value, unsigned char channel) {

    ADMUX = (1 << REFS0) | (channel & 0x0F); // Set reference to AVcc and select ADC channel (0-7)
    ADCSRA |= (1 << ADSC); // Start ADC conversion
    while (ADCSRA & (1 << ADSC)); // Wait for conversion to complete
    *value = ADC;
}