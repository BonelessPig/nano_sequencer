#ifndef ENV_OUTPUT_PORT_H
#define ENV_OUTPUT_PORT_H
/**
 * @file   env_output_port.h
 * @brief  Port: the envelope control voltage, which says how loud whatever
 *         the sequencer is playing is to be, from moment to moment.
 * @author BonelessPig
 *
 * @copyright Copyright (c) 2026
 *
 */
#include <stdint.h>
#include "port_status.h"

/**
 * @brief  Sets the envelope control voltage. It stays there until the next
 *         call. Writing the voltage it already has changes nothing, so it
 *         can be called on every tick.
 * @param  millivolts  Voltage to set, in nominal millivolts at the output of
 *                     the converter, 0 to 4095. Nominal because the
 *                     converter and whatever follows it are not exact, and
 *                     nothing calibrates this output.
 * @return STATUS_OK on success; ERR_INVALID_PARAM for a voltage above 4095,
 *         in which case nothing is sent; ERR_TIMEOUT if the hardware did not
 *         answer, in which case the voltage is left as it was.
 * @note   Not ISR-safe. Blocks for the duration of the write (about 20
 *         microseconds on the target).
 */
port_status_t env_output_write(uint16_t millivolts);

#endif /* ENV_OUTPUT_PORT_H */
