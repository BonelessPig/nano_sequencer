#ifndef UTILITIES_H
#define UTILITIES_H
/**
 * @file   utilities.h
 * @brief  Header file for utility functions and definitions.
 * @author BonelessPig
 * @date   2025-12-08
 *
 * @copyright Copyright (c) 2025
 *
 */
#include "varargs.h"

 void *memset(void *s, int c, unsigned int n);

 void *memmove(void *dest, const void *src, unsigned int n);

 /**
  * @brief  Writes a formatted string into a fixed-size buffer, in place of
  *         avr-libc's vsnprintf. Supports %d (signed decimal) and %% (literal
  *         percent); any other specifier is emitted literally rather than
  *         silently misinterpreted.
  * @param  str destination buffer
  * @param  size size of str, including the terminating null byte
  * @param  format printf-style format string
  * @param  args variadic arguments already captured via va_start
  * @return the number of characters that would have been written had size
  *         been unlimited (matching vsnprintf's contract), not counting the
  *         terminating null byte
  */
 int vsnprintf(char *str, unsigned int size, const char *format, va_list args);

#endif