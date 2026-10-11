/**
 * @file   reset_cause.c
 * @brief  Target implementation of the reset cause port: the MCU's reset
 *         flags, read and cleared.
 * @author BonelessPig
 *
 * The MCU sets one flag in MCUSR for each kind of reset and never clears
 * them itself, except that a power-on reset clears the others. In the
 * register they are, from bit 0: power-on, external (the reset pin),
 * brown-out, watchdog. That is the order the port gives them in, so the low
 * four bits are passed on as they are.
 *
 * @copyright Copyright (c) 2026
 *
 */
#include "reset_cause_port.h"
#include <stdint.h>
#include "atmega328p_regs.h"

#define RESET_FLAGS_MASK (0x0FU) // PORF, EXTRF, BORF and WDRF: bits 0 to 3



/**
 * @brief  Reads the reset flags and clears them, so that the next start-up
 *         sees only what caused it (see reset_cause_port.h).
 * @return uint8_t the flags, 0 to 15
 */
uint8_t reset_cause_read(void)
{
    const uint8_t flags = (uint8_t)(MCUSR & RESET_FLAGS_MASK);

    MCUSR = 0U;

    return flags;
}
