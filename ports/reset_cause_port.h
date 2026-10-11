#ifndef RESET_CAUSE_PORT_H
#define RESET_CAUSE_PORT_H
/**
 * @file   reset_cause_port.h
 * @brief  Port: why the processor last started.
 * @author BonelessPig
 *
 * @copyright Copyright (c) 2026
 *
 */
#include <stdint.h>

/**
 * @brief  Reports what has reset the processor since the previous call, and
 *         forgets it. Call once, at start-up.
 * @return One bit for each kind of reset that has happened; more than one
 *         can be set. Bit 0 (value 1): the power came on. Bit 1 (2): the
 *         reset line was pulled. Bit 2 (4): the supply dipped (brown-out).
 *         Bit 3 (8): the watchdog ran out. 0 means none is recorded, which
 *         is what a second call gives, and what a bootloader that clears
 *         the record itself leaves behind.
 * @note   Not ISR-safe. Never blocks.
 */
uint8_t reset_cause_read(void);

#endif /* RESET_CAUSE_PORT_H */
