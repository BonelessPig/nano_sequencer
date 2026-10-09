#ifndef LOG_PORT_H
#define LOG_PORT_H
/**
 * @file   log_port.h
 * @brief  Port: diagnostic output. The adapter owns the wording and the
 *         transport (serial on the target).
 * @author BonelessPig
 *
 * @copyright Copyright (c) 2026
 *
 */
#include <stdint.h>
#include "port_status.h"

/**
 * @brief What failed, for log_error().
 */
typedef enum
{
    LOG_ERROR_INIT = 0,   // platform_init() failed
    LOG_ERROR_STEP_READ,  // step_input_read() failed
    LOG_ERROR_TEMPO_READ  // tempo_input_read() failed
} log_error_id_t;

/**
 * @brief  Reports one step's note value (debug-level output).
 * @param  step  Step index, 0 to 15.
 * @param  note  Note value, 0 to 15.
 * @note   Not ISR-safe. Blocks until the message has been sent (about 20 ms
 *         per message at 9600 baud on the target).
 */
void log_step_note(uint8_t step, uint8_t note);

/**
 * @brief  Reports a failed operation (error-level output).
 * @param  what    Which operation failed.
 * @param  status  The status code it returned; printed as its number.
 * @note   Not ISR-safe. Blocks until the message has been sent.
 */
void log_error(log_error_id_t what, port_status_t status);

#endif /* LOG_PORT_H */
