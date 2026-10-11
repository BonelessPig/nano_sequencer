#ifndef LEVEL_OUTPUT_PORT_H
#define LEVEL_OUTPUT_PORT_H
/**
 * @file   level_output_port.h
 * @brief  Port: a light whose brightness shows a level.
 * @author BonelessPig
 *
 * @copyright Copyright (c) 2026
 *
 */
#include <stdint.h>

/**
 * @brief  Sets how bright the level light is. It stays there until the next
 *         call. Writing the level it already has changes nothing, so it can
 *         be called on every tick. platform_init() leaves the light dark.
 * @param  level  0 for dark and 255 for full brightness. In between, the
 *                light is on for (level + 1) parts in 256 of the time, a
 *                thousand times a second or so on the target.
 * @note   Not ISR-safe. Never blocks.
 */
void level_output_write(uint8_t level);

#endif /* LEVEL_OUTPUT_PORT_H */
