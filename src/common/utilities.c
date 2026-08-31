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



/**
 * @brief  Stores one character into str if there is room for it (plus the
 *         terminating null byte written later by the caller), and always
 *         counts it toward the total, matching vsnprintf's truncation contract.
 */
static void emit_char(char *str, unsigned int size, unsigned int *written, unsigned int *total, char c) {
    if (*written + 1 < size) str[(*written)++] = c;
    (*total)++;
}



/**
 * @brief  Writes a formatted string into a fixed-size buffer, in place of avr-libc's vsnprintf.
 * @param  str destination buffer
 * @param  size size of str, including the terminating null byte
 * @param  format printf-style format string
 * @param  args variadic arguments already captured via va_start
 * @return the number of characters that would have been written had size been unlimited
 */
int vsnprintf(char *str, unsigned int size, const char *format, va_list args) {
    unsigned int written = 0;
    unsigned int total = 0;

    for (const char *p = format; *p != '\0'; p++) {
        if (*p != '%') {
            emit_char(str, size, &written, &total, *p);
            continue;
        }

        p++;
        if (*p == '\0') {
            emit_char(str, size, &written, &total, '%'); // Trailing '%' with nothing after it
            break;
        }

        if (*p == '%') {
            emit_char(str, size, &written, &total, '%');
        } else if (*p == 'd') {
            int value = va_arg(args, int);
            unsigned int uvalue;
            char digits[5]; // Max digits for a 16-bit int magnitude (32768)
            unsigned char count = 0;

            if (value < 0) {
                emit_char(str, size, &written, &total, '-');
                uvalue = (unsigned int)(-(value + 1)) + 1; // Avoids overflow when value is INT_MIN
            } else {
                uvalue = (unsigned int)value;
            }

            do {
                digits[count++] = '0' + (uvalue % 10);
                uvalue /= 10;
            } while (uvalue > 0);

            while (count > 0) {
                emit_char(str, size, &written, &total, digits[--count]);
            }
        } else {
            // Unrecognized specifier: emit it literally rather than silently
            // misinterpreting it, since only %d and %% are actually supported
            emit_char(str, size, &written, &total, '%');
            emit_char(str, size, &written, &total, *p);
        }
    }

    if (size > 0) str[written] = '\0';
    return (int)total;
}