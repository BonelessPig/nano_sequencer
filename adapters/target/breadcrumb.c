/**
 * @file   breadcrumb.c
 * @brief  Target implementation of the breadcrumb port: one byte of RAM
 *         that the startup code leaves alone.
 * @author BonelessPig
 *
 * The startup code clears or sets every ordinary variable before main runs.
 * A variable placed in the .noinit section is skipped, so it still holds
 * what was last written to it when the processor is reset with the power
 * on. After power-on it holds whatever the RAM came up with.
 *
 * @copyright Copyright (c) 2026
 *
 */
#include "breadcrumb_port.h"
#include <stdint.h>
#include "atmega328p_regs.h"

static uint8_t g_breadcrumb RAM_NOINIT; // The place last noted; survives a reset



void breadcrumb_write(uint8_t place)
{
    g_breadcrumb = place;
}



uint8_t breadcrumb_read(void)
{
    return g_breadcrumb;
}
