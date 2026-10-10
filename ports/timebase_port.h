#ifndef TIMEBASE_PORT_H
#define TIMEBASE_PORT_H
/**
 * @file   timebase_port.h
 * @brief  Port: the passing of time, in ticks of one millisecond.
 * @author BonelessPig
 *
 * @copyright Copyright (c) 2026
 *
 */
#include <stdint.h>

/**
 * @brief  Reports how many ticks have passed since the previous call (or,
 *         on the first call, since platform_init()). Never blocks.
 * @return Elapsed ticks, 0 to 255. One tick is one millisecond. 0 means the
 *         next tick has not arrived yet. No tick is lost if the caller is
 *         late, as long as it calls again within 255 ms; beyond that the
 *         count wraps and whole multiples of 256 ticks go missing.
 * @note   Not ISR-safe, and for one caller only: each call consumes the
 *         ticks it reports.
 */
uint8_t timebase_elapsed_ticks(void);

#endif /* TIMEBASE_PORT_H */
