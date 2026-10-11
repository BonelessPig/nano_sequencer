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

#define HOST_RAW_STEP_BYTES (10U) // Size of the scripted step-input buffer
#define HOST_MAX_EVENTS     (64U) // Output-port calls recorded before further ones are dropped
#define HOST_NV_BYTES       (64U) // Size of the faked non-volatile storage
#define HOST_NV_ERASED      (0xFFU) // What a byte of it holds until it is written
#define HOST_BREADCRUMB_HISTORY (8U) // breadcrumb_write() calls kept, newest last

/**
 * @brief Which output-port call an event records.
 */
typedef enum
{
    HOST_EVENT_STEP_NOTE = 0, // log_step_note(step, semitone): a = step, b = semitone
    HOST_EVENT_STEP_REST,     // log_step_rest(step):           a = step, b = 0
    HOST_EVENT_ERROR,         // log_error(what, status):       a = what, b = status
    HOST_EVENT_FLUSH,         // log_flush():                   a = 0,    b = 0
    HOST_EVENT_CAL_POINT,     // log_cal_point(point, cv):      a = point, b = cv
    HOST_EVENT_TRANSPORT      // log_transport(mode):           a = mode, b = 0
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
 *        is 0, no time passes, the non-volatile storage is erased, no
 *        reset cause is recorded and the breadcrumb is 0. The record of
 *        MIDI bytes is emptied and the MIDI output has room for
 *        HOST_MIDI_ROOM_DEFAULT bytes.
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
 * @brief Sets what pot_input_read() returns.
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
 * @brief Sets what the next reset_cause_read() returns. Like the hardware,
 *        reading it clears it.
 * @param cause  Reset flags to report, 0 to 15.
 */
void host_set_reset_cause(uint8_t cause);

/**
 * @brief Sets what breadcrumb_read() returns, as if the firmware had noted
 *        that place before it was last reset. Like the non-volatile storage,
 *        the breadcrumb is kept when the app is initialized again.
 * @param place  Any value.
 */
void host_set_breadcrumb(uint8_t place);

/**
 * @brief The place breadcrumb_write() was last given, or host_set_breadcrumb()
 *        if that was later (0 if neither).
 */
uint8_t host_breadcrumb(void);

/**
 * @brief Number of times breadcrumb_write() has been called since
 *        host_reset().
 */
uint16_t host_breadcrumb_write_count(void);

/**
 * @brief  Returns the place one of the most recent breadcrumb_write() calls
 *         was given.
 * @param  age  0 for the latest call, 1 for the one before it, and so on up
 *              to HOST_BREADCRUMB_HISTORY - 1.
 * @return The place, or 0 if there have not been that many calls or age is
 *         out of range.
 */
uint8_t host_breadcrumb_before(uint8_t age);

/**
 * @brief The breadcrumb as it stood at the most recent cv_output_write()
 *        call (0 if none), which is the first thing the apply step does.
 */
uint8_t host_breadcrumb_at_cv_write(void);

/**
 * @brief The breadcrumb as it stood at the most recent run_input_read()
 *        call (0 if none), which is part of gathering the inputs.
 */
uint8_t host_breadcrumb_at_run_read(void);

/**
 * @brief Number of times log_crash() has been called since host_reset().
 *        Like log_startup(), it is kept out of the event record.
 */
uint16_t host_crash_count(void);

/**
 * @brief The record number the most recent log_crash() call was given (0 if
 *        none).
 */
uint8_t host_crash_number(void);

/**
 * @brief The place the most recent log_crash() call was given (0 if none).
 */
uint8_t host_crash_place(void);

/**
 * @brief The count the most recent log_crash() call was given (0 if none).
 */
uint8_t host_crash_repeats(void);

/**
 * @brief Puts an edge on the external clock input: the next
 *        clock_input_edge() returns true, and the ones after it false.
 */
void host_pulse_clock_input(void);

/**
 * @brief Puts an edge on the reset input: the next reset_input_edge()
 *        returns true, and the ones after it false.
 */
void host_pulse_reset_input(void);

/**
 * @brief Sets what run_input_read() returns until it is set again. Nothing
 *        bounces: the app's own debounce still makes it wait.
 * @param b_pressed  true to hold the run button down.
 */
void host_set_run_button(bool b_pressed);

/**
 * @brief Puts bytes straight into the non-volatile storage, as if written
 *        before the power last went off. Bytes that would fall outside the
 *        storage are left out.
 * @param address  Address of the first byte.
 * @param p_data   The bytes; null does nothing.
 * @param count    How many.
 */
void host_set_nv_bytes(uint16_t address, const uint8_t *p_data, uint8_t count);

/**
 * @brief  Returns one byte of the non-volatile storage.
 * @param  address  0 to HOST_NV_BYTES - 1.
 * @return The byte, or HOST_NV_ERASED if the address is outside the storage.
 */
uint8_t host_nv_byte(uint16_t address);

/**
 * @brief Sets the status nv_read() returns. Data is only copied out when it
 *        is STATUS_OK.
 */
void host_set_nv_read_status(port_status_t status);

/**
 * @brief Sets the status nv_write() returns. The bytes are only stored when
 *        it is STATUS_OK, and are then stored at once.
 */
void host_set_nv_write_status(port_status_t status);

/**
 * @brief Number of times nv_read() has been called since host_reset().
 */
uint16_t host_nv_read_count(void);

/**
 * @brief Number of times nv_write() has been called since host_reset().
 */
uint16_t host_nv_write_count(void);

/**
 * @brief Number of times nv_poll() has been called since host_reset().
 */
uint16_t host_nv_poll_count(void);

/**
 * @brief Number of times log_startup() has been called since host_reset().
 *        It is kept out of the event record, so that the record of a run
 *        begins with its first step.
 */
uint16_t host_startup_count(void);

/**
 * @brief The reset cause the most recent log_startup() call was given (0 if
 *        none).
 */
uint8_t host_startup_reset_cause(void);

/**
 * @brief Whether the most recent log_startup() call reported a stored
 *        calibration (false if none).
 */
bool host_startup_calibrated(void);

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
 * @brief Number of times pot_input_read() has been called since host_reset().
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
 * @brief The level level_output_write() was last given (0 if never called).
 */
uint8_t host_level(void);

/**
 * @brief Number of times level_output_write() has been called since host_reset().
 */
uint16_t host_level_write_count(void);

/**
 * @brief Number of times watchdog_kick() has been called since host_reset().
 */
uint16_t host_watchdog_kick_count(void);

/**
 * @brief What led_output_write() was last given (0 if never called): one bit
 *        a step light.
 */
uint16_t host_leds(void);

/**
 * @brief Number of times led_output_write() has been called since host_reset().
 */
uint16_t host_led_write_count(void);

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

#define HOST_MIDI_BYTES_MAX    (1024U) // MIDI bytes recorded before further ones are dropped
#define HOST_MIDI_ROOM_DEFAULT (255U)  // What midi_output_room() gives until it is set

/**
 * @brief Sets what midi_output_room() returns until it is set again.
 *        host_reset() puts it back to HOST_MIDI_ROOM_DEFAULT. The fake does
 *        not hold the app to it: a write of more than the room is recorded
 *        whole, so that a test can see it happened.
 * @param room  Bytes to report as free, 0 to 255.
 */
void host_set_midi_room(uint8_t room);

/**
 * @brief Number of times midi_output_room() has been called since
 *        host_reset().
 */
uint16_t host_midi_room_read_count(void);

/**
 * @brief Number of times midi_output_write() has been called since
 *        host_reset(), calls that gave no bytes included.
 */
uint16_t host_midi_write_count(void);

/**
 * @brief Number of bytes midi_output_write() has been given since
 *        host_reset() and the fake has kept: it stops at
 *        HOST_MIDI_BYTES_MAX. They are kept out of the event record.
 */
uint16_t host_midi_byte_count(void);

/**
 * @brief  Returns a byte midi_output_write() was given.
 * @param  index  0 is the oldest; must be below host_midi_byte_count().
 * @return The byte, or 0 if index is out of range.
 */
uint8_t host_midi_byte(uint16_t index);

/**
 * @brief Number of times midi_output_poll() has been called since
 *        host_reset(). The fake sends nothing: a byte is recorded when it
 *        is written, not when it is polled.
 */
uint16_t host_midi_poll_count(void);

/**
 * @brief What host_midi_byte_count() was when the gate last went from low
 *        to high: 0 if the MIDI bytes of that tick came after the gate.
 */
uint16_t host_midi_byte_count_at_gate_rise(void);

/**
 * @brief What host_midi_byte_count() was when the first event since
 *        host_reset() was recorded: the bytes that were written ahead of
 *        the log.
 */
uint16_t host_midi_byte_count_at_first_event(void);

#endif /* HOST_PORTS_H */
