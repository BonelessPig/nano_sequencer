#ifndef HOST_PORTS_H
#define HOST_PORTS_H
/**
 * @file   host_ports.h
 * @brief  Control interface for the host (PC) fakes of the ports. Tests use
 *         it to script what the input ports return and to inspect what the
 *         output ports were asked to do, in the order it happened.
 * @author BonelessPig
 *
 * @copyright Copyright (c) 2026
 *
 */
#include <stdbool.h>
#include <stdint.h>
#include "log_port.h"
#include "port_status.h"

#define HOST_RAW_STEP_BYTES (8U)  // Size of the scripted step-input buffer
#define HOST_MAX_EVENTS     (64U) // Output-port calls recorded before further ones are dropped

/**
 * @brief Which output-port call an event records.
 */
typedef enum
{
    HOST_EVENT_STEP_NOTE = 0, // log_step_note(step, semitone): a = step, b = semitone
    HOST_EVENT_STEP_REST,     // log_step_rest(step):           a = step, b = 0
    HOST_EVENT_ERROR,         // log_error(what, status):       a = what, b = status
    HOST_EVENT_FLUSH          // log_flush():                   a = 0,    b = 0
} host_event_kind_t;

/**
 * @brief One recorded output-port call.
 */
typedef struct
{
    host_event_kind_t kind;
    uint16_t          a; // First argument (see host_event_kind_t)
    uint16_t          b; // Second argument (see host_event_kind_t)
} host_event_t;

/**
 * @brief Clears the event record and the call counts, puts the gate and the
 *        clock output low and the pitch CV at 0, and restores the default
 *        script: every port succeeds, all step bits are 0, the tempo reading
 *        is 0 and no time passes.
 */
void host_reset(void);

/**
 * @brief Sets the status platform_init() returns.
 */
void host_set_platform_status(port_status_t status);

/**
 * @brief Sets what step_input_read() returns.
 * @param p_raw_bits  HOST_RAW_STEP_BYTES bytes to hand out; null leaves the data unchanged.
 * @param status      Status to return. Data is only copied out when it is STATUS_OK.
 */
void host_set_steps(const uint8_t *p_raw_bits, port_status_t status);

/**
 * @brief Sets what tempo_input_read() returns.
 * @param raw     Reading to hand out, 0 to 1023.
 * @param status  Status to return. The reading is only written when it is STATUS_OK.
 */
void host_set_tempo(uint16_t raw, port_status_t status);

/**
 * @brief Sets the status cv_output_write() returns. The voltage is only
 *        taken when it is STATUS_OK.
 */
void host_set_cv_status(port_status_t status);

/**
 * @brief Sets what timebase_elapsed_ticks() returns, on every call until it
 *        is set again.
 * @param ticks  Elapsed ticks to report, 0 to 255.
 */
void host_set_elapsed_ticks(uint8_t ticks);

/**
 * @brief Number of output-port calls recorded since host_reset().
 */
uint16_t host_event_count(void);

/**
 * @brief  Returns a recorded output-port call.
 * @param  index  0 is the oldest; must be below host_event_count().
 * @return The event, or null if index is out of range.
 */
const host_event_t *host_event_get(uint16_t index);

/**
 * @brief Number of times platform_init() has been called since host_reset().
 */
uint16_t host_platform_init_count(void);

/**
 * @brief The byte_count passed to the most recent step_input_read() call (0 if none).
 */
uint8_t host_last_step_byte_count(void);

/**
 * @brief Number of times step_input_read() has been called since host_reset().
 */
uint16_t host_step_read_count(void);

/**
 * @brief Number of times tempo_input_read() has been called since host_reset().
 */
uint16_t host_tempo_read_count(void);

/**
 * @brief Number of times log_poll() has been called since host_reset().
 */
uint16_t host_log_poll_count(void);

/**
 * @brief The level gate_output_write() was last given (false if never called).
 */
bool host_gate_level(void);

/**
 * @brief Number of times gate_output_write() has been called since host_reset().
 */
uint16_t host_gate_write_count(void);

/**
 * @brief Number of times the gate has gone from low to high since host_reset().
 */
uint16_t host_gate_rise_count(void);

/**
 * @brief What host_event_count() was when the gate last went high (0 if it
 *        never has), which shows whether the gate or the log came first.
 */
uint16_t host_event_count_at_gate_rise(void);

/**
 * @brief The voltage, in millivolts, of the last cv_output_write() call that
 *        succeeded (0 if none has).
 */
uint16_t host_cv_millivolts(void);

/**
 * @brief Number of times cv_output_write() has been called since host_reset().
 */
uint16_t host_cv_write_count(void);

/**
 * @brief What host_cv_millivolts() was when the gate last went high (0 if it
 *        never has), which shows whether the pitch was in place first.
 */
uint16_t host_cv_millivolts_at_gate_rise(void);

/**
 * @brief The level clock_output_write() was last given (false if never called).
 */
bool host_clock_output_level(void);

/**
 * @brief Number of times the clock output has gone from low to high since
 *        host_reset().
 */
uint16_t host_clock_output_rise_count(void);

#endif /* HOST_PORTS_H */
