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
#include "delay_port.h"
#include "platform_port.h"
#include "step_input_port.h"
#include "tempo_input_port.h"

#define STEP_BYTE_COUNT_MIN (1U)

static port_status_t g_platform_status = STATUS_OK;
static uint16_t      g_platform_init_count = 0U;

static uint8_t       g_raw_steps[HOST_RAW_STEP_BYTES];
static port_status_t g_steps_status = STATUS_OK;
static uint8_t       g_last_step_byte_count = 0U;

static uint16_t      g_tempo_raw = 0U;
static port_status_t g_tempo_status = STATUS_OK;

static host_event_t  g_events[HOST_MAX_EVENTS];
static uint16_t      g_event_count = 0U;



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
    g_tempo_raw            = 0U;
    g_tempo_status         = STATUS_OK;
    g_event_count          = 0U;

    for (uint8_t i = 0U; i < HOST_RAW_STEP_BYTES; i++)
    {
        g_raw_steps[i] = 0U;
    }
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



port_status_t platform_init(void)
{
    g_platform_init_count++;
    return g_platform_status;
}



port_status_t step_input_read(uint8_t *p_raw_bits, uint8_t byte_count)
{
    port_status_t status = g_steps_status;

    g_last_step_byte_count = byte_count;

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



void delay_wait_ms(uint16_t ms)
{
    record_event(HOST_EVENT_DELAY, ms, 0U);
}



void log_step_note(uint8_t step, uint8_t note)
{
    record_event(HOST_EVENT_STEP_NOTE, step, note);
}



void log_error(log_error_id_t what, port_status_t status)
{
    record_event(HOST_EVENT_ERROR, (uint16_t)what, (uint16_t)status);
}
