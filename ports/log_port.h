#ifndef LOG_PORT_H
#define LOG_PORT_H
/**
 * @file   log_port.h
 * @brief  Port: diagnostic output. The adapter owns the wording and the
 *         transport (serial in the target's debug build; the release build
 *         discards every message). No function here blocks: a message is
 *         queued, and log_poll() sends it a little at a time.
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
 * @brief  Reports the step being played and its note value (debug-level output).
 * @param  step  Step index, 0 to 15.
 * @param  note  Note value, 0 to 15.
 * @note   Not ISR-safe. Does not block: the message is queued for log_poll()
 *         to send. If the queue has no room for the whole message, the
 *         message is dropped.
 */
void log_step_note(uint8_t step, uint8_t note);

/**
 * @brief  Reports a failed operation (error-level output).
 * @param  what    Which operation failed.
 * @param  status  The status code it returned; printed as its number.
 * @note   Not ISR-safe. Does not block; queued or dropped like
 *         log_step_note().
 */
void log_error(log_error_id_t what, port_status_t status);

/**
 * @brief  Sends at most one queued byte, if the transport is ready for it.
 *         Call on every pass of the main loop.
 * @note   Not ISR-safe. Never blocks. On the target's debug build one byte
 *         takes about 85 microseconds at 115200 baud, so a 19-character line
 *         is gone about 1.6 ms after it was queued.
 */
void log_poll(void);

/**
 * @brief  Sends everything still queued, waiting for the transport as long as
 *         that takes. Only for use when the main loop is about to stop and
 *         log_poll() will not be called again.
 * @note   Not ISR-safe. Blocks: about 85 microseconds per queued byte on the
 *         target's debug build.
 */
void log_flush(void);

#endif /* LOG_PORT_H */
