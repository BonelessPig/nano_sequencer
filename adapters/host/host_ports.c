/**
 * @file   host_ports.c
 * @brief  Host (PC) implementations of every port, for tests. Input ports
 *         return whatever the test scripted; output ports record their calls
 *         in order. Nothing here blocks or touches hardware.
 * @author BonelessPig
 *
 * @copyright Copyright (c) 2026
 *
 */
#include "host_ports.h"
#include <stddef.h>
#include "clock_input_port.h"
#include "clock_output_port.h"
#include "cv_output_port.h"
#include "gate_output_port.h"
#include "led_output_port.h"
#include "nv_port.h"
#include "platform_port.h"
#include "reset_cause_port.h"
#include "reset_input_port.h"
#include "run_input_port.h"
#include "step_input_port.h"
#include "sync_input_port.h"
#include "tempo_input_port.h"
#include "timebase_port.h"

#define STEP_BYTE_COUNT_MIN (1U)

static port_status_t g_platform_status = STATUS_OK;
static uint16_t      g_platform_init_count = 0U;

static uint8_t       g_raw_steps[HOST_RAW_STEP_BYTES];
static port_status_t g_steps_status = STATUS_OK;
static uint8_t       g_last_step_byte_count = 0U;
static uint16_t      g_step_read_count = 0U;

static uint16_t      g_tempo_raw = 0U;
static port_status_t g_tempo_status = STATUS_OK;
static uint16_t      g_tempo_read_count = 0U;

static uint8_t       g_elapsed_ticks = 0U;
static uint16_t      g_log_poll_count = 0U;

static bool          g_b_gate_high = false;
static uint16_t      g_host_leds = 0U;
static uint16_t      g_led_write_count = 0U;
static uint16_t      g_gate_write_count = 0U;
static uint16_t      g_gate_rise_count = 0U;
static uint16_t      g_event_count_at_gate_rise = 0U;

static bool          g_b_clock_output_high = false;
static uint16_t      g_clock_output_rise_count = 0U;

static port_status_t g_cv_status = STATUS_OK;
static uint16_t      g_cv_millivolts = 0U;
static uint16_t      g_cv_write_count = 0U;
static uint16_t      g_cv_millivolts_at_gate_rise = 0U;

static uint8_t       g_reset_cause = 0U;

static bool          g_b_clock_edge = false;
static bool          g_b_reset_edge = false;
static bool          g_b_run_button = false;
static bool          g_b_sync_switch = false;

static uint8_t       g_nv_bytes[HOST_NV_BYTES];
static port_status_t g_nv_read_status = STATUS_OK;
static port_status_t g_nv_write_status = STATUS_OK;
static uint16_t      g_nv_read_count = 0U;
static uint16_t      g_nv_write_count = 0U;
static uint16_t      g_nv_poll_count = 0U;

static uint16_t      g_startup_count = 0U;
static uint8_t       g_startup_reset_cause = 0U;
static bool          g_b_startup_calibrated = false;

static host_event_t  g_events[HOST_MAX_EVENTS];
static uint16_t      g_event_count = 0U;



/**
 * @brief Tells whether a run of bytes lies inside the faked storage.
 */
static bool is_nv_run_valid(uint16_t address, uint8_t count)
{
    return (count > 0U) && (address < HOST_NV_BYTES) &&
           (count <= (uint16_t)(HOST_NV_BYTES - address));
}



/**
 * @brief Appends one output-port call to the record; dropped if the record is full.
 */
static void record_event(host_event_kind_t kind, uint16_t a, uint16_t b)
{
    if (g_event_count < HOST_MAX_EVENTS)
    {
        g_events[g_event_count].kind = kind;
        g_events[g_event_count].a    = a;
        g_events[g_event_count].b    = b;
        g_event_count++;
    }
}



void host_reset(void)
{
    g_platform_status      = STATUS_OK;
    g_platform_init_count  = 0U;
    g_steps_status         = STATUS_OK;
    g_last_step_byte_count = 0U;
    g_step_read_count      = 0U;
    g_tempo_raw            = 0U;
    g_tempo_status         = STATUS_OK;
    g_tempo_read_count     = 0U;
    g_elapsed_ticks        = 0U;
    g_log_poll_count       = 0U;
    g_event_count          = 0U;

    g_b_gate_high              = false;
    g_host_leds                = 0U;
    g_led_write_count          = 0U;
    g_gate_write_count         = 0U;
    g_gate_rise_count          = 0U;
    g_event_count_at_gate_rise = 0U;
    g_b_clock_output_high      = false;
    g_clock_output_rise_count  = 0U;

    g_cv_status                  = STATUS_OK;
    g_cv_millivolts              = 0U;
    g_cv_write_count             = 0U;
    g_cv_millivolts_at_gate_rise = 0U;

    g_reset_cause          = 0U;
    g_b_clock_edge         = false;
    g_b_reset_edge         = false;
    g_b_run_button         = false;
    g_b_sync_switch        = false;
    g_nv_read_status       = STATUS_OK;
    g_nv_write_status      = STATUS_OK;
    g_nv_read_count        = 0U;
    g_nv_write_count       = 0U;
    g_nv_poll_count        = 0U;
    g_startup_count        = 0U;
    g_startup_reset_cause  = 0U;
    g_b_startup_calibrated = false;

    for (uint8_t i = 0U; i < HOST_RAW_STEP_BYTES; i++)
    {
        g_raw_steps[i] = 0U;
    }
    for (uint8_t i = 0U; i < HOST_NV_BYTES; i++)
    {
        g_nv_bytes[i] = HOST_NV_ERASED;
    }
}



void host_set_reset_cause(uint8_t cause)
{
    g_reset_cause = cause;
}



void host_pulse_clock_input(void)
{
    g_b_clock_edge = true;
}



void host_pulse_reset_input(void)
{
    g_b_reset_edge = true;
}



void host_set_run_button(bool b_pressed)
{
    g_b_run_button = b_pressed;
}



void host_set_sync_switch(bool b_on)
{
    g_b_sync_switch = b_on;
}



void host_set_nv_bytes(uint16_t address, const uint8_t *p_data, uint8_t count)
{
    if (NULL != p_data)
    {
        for (uint8_t i = 0U; i < count; i++)
        {
            const uint16_t at = (uint16_t)(address + i);

            if (at < HOST_NV_BYTES)
            {
                g_nv_bytes[at] = p_data[i];
            }
        }
    }
}



uint8_t host_nv_byte(uint16_t address)
{
    uint8_t value = HOST_NV_ERASED;

    if (address < HOST_NV_BYTES)
    {
        value = g_nv_bytes[address];
    }
    return value;
}



void host_set_nv_read_status(port_status_t status)
{
    g_nv_read_status = status;
}



void host_set_nv_write_status(port_status_t status)
{
    g_nv_write_status = status;
}



uint16_t host_nv_read_count(void)
{
    return g_nv_read_count;
}



uint16_t host_nv_write_count(void)
{
    return g_nv_write_count;
}



uint16_t host_nv_poll_count(void)
{
    return g_nv_poll_count;
}



uint16_t host_startup_count(void)
{
    return g_startup_count;
}



uint8_t host_startup_reset_cause(void)
{
    return g_startup_reset_cause;
}



bool host_startup_calibrated(void)
{
    return g_b_startup_calibrated;
}



void host_set_platform_status(port_status_t status)
{
    g_platform_status = status;
}



void host_set_steps(const uint8_t *p_raw_bits, port_status_t status)
{
    if (NULL != p_raw_bits)
    {
        for (uint8_t i = 0U; i < HOST_RAW_STEP_BYTES; i++)
        {
            g_raw_steps[i] = p_raw_bits[i];
        }
    }
    g_steps_status = status;
}



void host_set_tempo(uint16_t raw, port_status_t status)
{
    g_tempo_raw    = raw;
    g_tempo_status = status;
}



void host_set_elapsed_ticks(uint8_t ticks)
{
    g_elapsed_ticks = ticks;
}



void host_set_cv_status(port_status_t status)
{
    g_cv_status = status;
}



uint16_t host_event_count(void)
{
    return g_event_count;
}



const host_event_t *host_event_get(uint16_t index)
{
    const host_event_t *p_event = NULL;

    if (index < g_event_count)
    {
        p_event = &g_events[index];
    }
    return p_event;
}



uint16_t host_platform_init_count(void)
{
    return g_platform_init_count;
}



uint8_t host_last_step_byte_count(void)
{
    return g_last_step_byte_count;
}



uint16_t host_step_read_count(void)
{
    return g_step_read_count;
}



uint16_t host_tempo_read_count(void)
{
    return g_tempo_read_count;
}



uint16_t host_log_poll_count(void)
{
    return g_log_poll_count;
}



bool host_gate_level(void)
{
    return g_b_gate_high;
}



uint16_t host_leds(void)
{
    return g_host_leds;
}



uint16_t host_led_write_count(void)
{
    return g_led_write_count;
}



uint16_t host_gate_write_count(void)
{
    return g_gate_write_count;
}



uint16_t host_gate_rise_count(void)
{
    return g_gate_rise_count;
}



uint16_t host_event_count_at_gate_rise(void)
{
    return g_event_count_at_gate_rise;
}



uint16_t host_cv_millivolts(void)
{
    return g_cv_millivolts;
}



uint16_t host_cv_write_count(void)
{
    return g_cv_write_count;
}



uint16_t host_cv_millivolts_at_gate_rise(void)
{
    return g_cv_millivolts_at_gate_rise;
}



bool host_clock_output_level(void)
{
    return g_b_clock_output_high;
}



uint16_t host_clock_output_rise_count(void)
{
    return g_clock_output_rise_count;
}



port_status_t platform_init(void)
{
    g_platform_init_count++;
    return g_platform_status;
}



port_status_t step_input_read(uint8_t *p_raw_bits, uint8_t byte_count)
{
    port_status_t status = g_steps_status;

    g_last_step_byte_count = byte_count;
    g_step_read_count++;

    // Same parameter contract as the real adapter (see step_input_port.h)
    if ((NULL == p_raw_bits) || (byte_count < STEP_BYTE_COUNT_MIN) || (byte_count > HOST_RAW_STEP_BYTES))
    {
        status = ERR_INVALID_PARAM;
    }
    else if (STATUS_OK == status)
    {
        for (uint8_t i = 0U; i < byte_count; i++)
        {
            p_raw_bits[i] = g_raw_steps[i];
        }
    }
    else
    {
        // Scripted failure: leave the caller's buffer untouched
    }
    return status;
}



port_status_t tempo_input_read(uint16_t *p_raw)
{
    port_status_t status = g_tempo_status;

    g_tempo_read_count++;

    if (NULL == p_raw)
    {
        status = ERR_INVALID_PARAM;
    }
    else if (STATUS_OK == status)
    {
        *p_raw = g_tempo_raw;
    }
    else
    {
        // Scripted failure: leave the caller's value untouched
    }
    return status;
}



uint8_t timebase_elapsed_ticks(void)
{
    return g_elapsed_ticks;
}



port_status_t cv_output_write(uint16_t millivolts)
{
    port_status_t status = g_cv_status;

    g_cv_write_count++;

    // Same parameter contract as the real adapter (see cv_output_port.h)
    if (millivolts > CV_OUTPUT_MILLIVOLTS_MAX)
    {
        status = ERR_INVALID_PARAM;
    }
    else if (STATUS_OK == status)
    {
        g_cv_millivolts = millivolts;
    }
    else
    {
        // Scripted failure: the voltage stays as it was
    }
    return status;
}



void led_output_write(uint16_t leds)
{
    g_led_write_count++;
    g_host_leds = leds;
}



void gate_output_write(bool b_high)
{
    g_gate_write_count++;
    if (b_high && !g_b_gate_high)
    {
        g_gate_rise_count++;
        g_event_count_at_gate_rise   = g_event_count;
        g_cv_millivolts_at_gate_rise = g_cv_millivolts;
    }
    g_b_gate_high = b_high;
}



void clock_output_write(bool b_high)
{
    if (b_high && !g_b_clock_output_high)
    {
        g_clock_output_rise_count++;
    }
    g_b_clock_output_high = b_high;
}



uint8_t reset_cause_read(void)
{
    const uint8_t cause = g_reset_cause;

    g_reset_cause = 0U; // Reading it clears it, as on the hardware
    return cause;
}



bool clock_input_edge(void)
{
    const bool b_edge = g_b_clock_edge;

    g_b_clock_edge = false; // Reading it forgets it, as on the hardware
    return b_edge;
}



bool reset_input_edge(void)
{
    const bool b_edge = g_b_reset_edge;

    g_b_reset_edge = false; // Reading it forgets it, as on the hardware
    return b_edge;
}



bool run_input_read(void)
{
    return g_b_run_button;
}



bool sync_input_read(void)
{
    return g_b_sync_switch;
}



port_status_t nv_read(uint16_t address, uint8_t *p_data, uint8_t count)
{
    port_status_t status = g_nv_read_status;

    g_nv_read_count++;

    // Same parameter contract as the real adapter (see nv_port.h)
    if ((NULL == p_data) || !is_nv_run_valid(address, count))
    {
        status = ERR_INVALID_PARAM;
    }
    else if (STATUS_OK == status)
    {
        for (uint8_t i = 0U; i < count; i++)
        {
            p_data[i] = g_nv_bytes[address + i];
        }
    }
    else
    {
        // Scripted failure: leave the caller's buffer untouched
    }
    return status;
}



port_status_t nv_write(uint16_t address, const uint8_t *p_data, uint8_t count)
{
    port_status_t status = g_nv_write_status;

    g_nv_write_count++;

    // Same parameter contract as the real adapter (see nv_port.h)
    if ((NULL == p_data) || (count > NV_WRITE_BYTES_MAX) ||
        !is_nv_run_valid(address, count))
    {
        status = ERR_INVALID_PARAM;
    }
    else if (STATUS_OK == status)
    {
        // Stored at once: the fake has no write time to wait out
        for (uint8_t i = 0U; i < count; i++)
        {
            g_nv_bytes[address + i] = p_data[i];
        }
    }
    else
    {
        // Scripted failure: the storage stays as it was
    }
    return status;
}



void nv_poll(void)
{
    g_nv_poll_count++;
}



void log_startup(uint8_t reset_cause, bool b_calibrated)
{
    g_startup_count++;
    g_startup_reset_cause  = reset_cause;
    g_b_startup_calibrated = b_calibrated;
}



void log_cal_point(uint8_t point, uint16_t cv)
{
    record_event(HOST_EVENT_CAL_POINT, point, cv);
}



void log_transport(uint8_t mode)
{
    record_event(HOST_EVENT_TRANSPORT, mode, 0U);
}



void log_step_note(uint8_t step, uint8_t semitone)
{
    record_event(HOST_EVENT_STEP_NOTE, step, semitone);
}



void log_step_rest(uint8_t step)
{
    record_event(HOST_EVENT_STEP_REST, step, 0U);
}



void log_error(log_error_id_t what, port_status_t status)
{
    record_event(HOST_EVENT_ERROR, (uint16_t)what, (uint16_t)status);
}



void log_poll(void)
{
    g_log_poll_count++;
}



void log_flush(void)
{
    record_event(HOST_EVENT_FLUSH, 0U, 0U);
}
