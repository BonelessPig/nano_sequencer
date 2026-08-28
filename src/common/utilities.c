/**
 * @file utilities.c
 * @author your name (you@domain.com)
 * @brief 
 * @version 0.1
 * @date 2025-12-19
 * 
 * @copyright Copyright (c) 2025
 * 
 */
#include "utilities.h"



/**
 * @brief  Delays execution for a specified number of milliseconds.
 * @param  millis Number of milliseconds to delay.
 */ 
void delay_ms(unsigned int millis) {
    while (millis--) {
        delay_clocks(clock_cycles_per_ms); // Assuming a 16 MHz clock, 16000 clocks = 1 ms
    }
}



/**
 * @brief  Fills the first n bytes of the memory area pointed to by s with the constant byte c.
 * @param  s Pointer to the memory area to fill.
 * @param  c The byte value to set (converted to unsigned char).
 * @param  n Number of bytes to be set to the value.
 * @return A pointer to the memory area s.
 */
void *memset(void *s, int c, unsigned int n) {
    unsigned char *p = s;
    while (n--) {
        *p++ = (unsigned char)c;
    }
    return s;
}  



/**
 * @brief  Copies n bytes from memory area src to memory area dest. The memory areas may overlap.
 * @param  dest Pointer to the destination memory area.
 * @param  src Pointer to the source memory area.
 * @param  n Number of bytes to copy.
 * @return A pointer to the destination memory area dest.
 */
void *memmove(void *dest, const void *src, unsigned int n) {
    unsigned char *d = dest;
    const unsigned char *s = src;

    if (d < s) {
        while (n--) {
            *d++ = *s++;
        }
    } else {
        d += n;
        s += n;
        while (n--) {
            *--d = *--s;
        }
    }
    return dest;
}