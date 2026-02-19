#ifndef __ANALOG_READER_H__
#define __ANALOG_READER_H__
/**
 * @file analog_reader.h
 * @author BonelessPig 
 * @brief reads the value of an analog input pin and prints it to serial
 * @version 0.1
 * @date 2026-02-18
 * 
 * @copyright Copyright (c) 2026
 * 
 */

/**
 * @brief Reads the value of an analog input pin and stores it in the provided variable.
 * @param value pointer to a short variable where the ADC value will be stored
 * @param channel the ADC channel to read from (0-7 for ATmega328P)
 */
void read_analog_value(short* value, unsigned char channel);

#endif