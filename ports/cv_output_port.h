#ifndef CV_OUTPUT_PORT_H
#define CV_OUTPUT_PORT_H
/**
 * @file   cv_output_port.h
 * @brief  Port: the pitch control voltage, which sets the pitch of whatever
 *         the sequencer is playing.
 * @author BonelessPig
 *
 * @copyright Copyright (c) 2026
 *
 */
#include <stdint.h>
#include "port_status.h"

#define CV_OUTPUT_MILLIVOLTS_MAX (4095U) // The highest voltage that can be asked for

/**
 * @brief  Sets the pitch control voltage. It stays there until the next
 *         call. Writing the voltage it already has changes nothing, so it
 *         can be called on every tick.
 * @param  millivolts  Voltage to set, in nominal millivolts at the output of
 *                     the converter, 0 to CV_OUTPUT_MILLIVOLTS_MAX. Nominal
 *                     because the converter and whatever follows it are not
 *                     exact; the caller's calibration allows for that.
 * @return STATUS_OK on success; ERR_INVALID_PARAM for a voltage above
 *         CV_OUTPUT_MILLIVOLTS_MAX, in which case nothing is sent;
 *         ERR_TIMEOUT if the hardware did not answer, in which case the
 *         voltage is left as it was.
 * @note   Not ISR-safe. Blocks for the duration of the write (about 20
 *         microseconds on the target).
 */
port_status_t cv_output_write(uint16_t millivolts);

#endif /* CV_OUTPUT_PORT_H */
