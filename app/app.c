/**
 * @file   app.c
 * @brief  The application: wires the ports to the sequencer core. This is the
 *         only place that knows about both.
 * @author BonelessPig
 *
 * @copyright Copyright (c) 2026
 *
 */
#include "app.h"
#include <stdbool.h>
#include <stdint.h>
#include "seq.h"
#include "delay_port.h"
#include "log_port.h"
#include "platform_port.h"
#include "step_input_port.h"
#include "tempo_input_port.h"

static seq_state_t g_seq_state; // Sequencer state carried between ticks



/**
 * @brief  Reads every input port into one snapshot for the core.
 * @param  p_in            Receives the snapshot; every field is written.
 * @param  p_steps_status  Receives the status of the step input read.
 * @param  p_tempo_status  Receives the status of the tempo input read.
 */
static void gather_inputs(seq_inputs_t *p_in, port_status_t *p_steps_status, port_status_t *p_tempo_status)
{
    for (uint8_t i = 0U; i < SEQ_RAW_BYTE_COUNT; i++)
    {
        p_in->raw_steps[i] = 0U;
    }
    p_in->tempo_raw = 0U;

    *p_steps_status = step_input_read(p_in->raw_steps, (uint8_t)SEQ_RAW_BYTE_COUNT);
    p_in->b_steps_valid = (STATUS_OK == *p_steps_status);

    *p_tempo_status = tempo_input_read(&p_in->tempo_raw);
    p_in->b_tempo_valid = (STATUS_OK == *p_tempo_status);
}



/**
 * @brief  Acts on the core's outputs: logs the step played (or the read
 *         failures), then waits out the delay.
 * @param  p_out         Outputs from this tick.
 * @param  steps_status  Status of this tick's step input read.
 * @param  tempo_status  Status of this tick's tempo input read.
 */
static void apply_outputs(const seq_outputs_t *p_out, port_status_t steps_status, port_status_t tempo_status)
{
    if (p_out->b_note_valid)
    {
        log_step_note(p_out->step, p_out->note);
    }
    else
    {
        log_error(LOG_ERROR_STEP_READ, steps_status);
    }

    if (STATUS_OK != tempo_status)
    {
        log_error(LOG_ERROR_TEMPO_READ, tempo_status);
    }

    delay_wait_ms(p_out->delay_ms);
}



port_status_t app_init(void)
{
    const port_status_t status = platform_init();

    if (STATUS_OK != status)
    {
        log_error(LOG_ERROR_INIT, status);
    }
    seq_init(&g_seq_state);

    return status;
}



void app_run_once(void)
{
    seq_inputs_t  in;
    seq_outputs_t out;
    port_status_t steps_status = STATUS_OK;
    port_status_t tempo_status = STATUS_OK;

    gather_inputs(&in, &steps_status, &tempo_status);
    seq_tick(&g_seq_state, &in, &out);
    apply_outputs(&out, steps_status, tempo_status);
}
