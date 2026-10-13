#ifndef STATUS_OUTPUT_PORT_H
#define STATUS_OUTPUT_PORT_H
/**
 * @file   status_output_port.h
 * @brief  Port: the status light, which is lit or dark.
 * @author BonelessPig
 *
 * @copyright Copyright (c) 2026
 *
 */
#include <stdbool.h>

/**
 * @brief  Lights the status light or puts it out. It stays so until the
 *         next call. Writing the level it already has changes nothing, so
 *         it can be called on every tick. platform_init() leaves it dark.
 * @param  b_lit  true for lit, false for dark.
 * @note   Not ISR-safe. Never blocks.
 */
void status_output_write(bool b_lit);

#endif /* STATUS_OUTPUT_PORT_H */
