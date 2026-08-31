#ifndef __VARARGS_H__
#define __VARARGS_H__
/**
 * @file   varargs.h
 * @brief  Variadic-argument support built on avr-gcc's builtins, in place of <stdarg.h>.
 * @author BonelessPig
 *
 * @copyright Copyright (c) 2026
 *
 */

typedef __builtin_va_list va_list;

#define va_start(v, l)  __builtin_va_start(v, l)
#define va_arg(v, type) __builtin_va_arg(v, type)
#define va_end(v)       __builtin_va_end(v)

#endif
