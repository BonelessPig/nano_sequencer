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
#include "log_port.h"
#include "platform_port.h"
#include "step_input_port.h"
#include "tempo_input_port.h"
#include "timebase_port.h"

// The panel and the tempo control are read once every this many ticks
// (125 times a second), not on every tick
#define INPUT_SCAN_PERIOD_TICKS (8U)

static seq_state_t   g_seq_state; // Sequencer state carried between ticks
static seq_inputs_t  g_inputs;    // Latest snapshot of the inputs
static seq_outputs_t g_outputs;   // What the last tick computed; applied at the next

static port_status_t g_step_read_status  = STATUS_OK; // Status of the last step input read
static port_status_t g_tempo_read_status = STATUS_OK; // Status of the last tempo input read
static uint8_t       g_ticks_until_scan  = 0U;        // Ticks before the inputs are read again



/**
 * @brief  Acts on what the previous tick computed: when a step began, logs it
 *         (or the failed step read), and a failed tempo read with it.
 */
static void apply_outputs(void)
{
    if (g_outputs.b_step_started)
    {
        if (g_outputs.b_note_valid)
        {
            log_step_note(g_outputs.step, g_outputs.note);
        }
        else
        {
            log_error(LOG_ERROR_STEP_READ, g_step_read_status);
        }

        if (STATUS_OK != g_tempo_read_status)
        {
            log_error(LOG_ERROR_TEMPO_READ, g_tempo_read_status);
        }
    }
}



/**
 * @brief  Reads the step and tempo input ports into the snapshot. A failed
 *         read clears that input's valid flag and leaves its data as it was.
 */
static void scan_inputs(void)
{
    g_step_read_status = step_input_read(g_inputs.raw_steps, (uint8_t)SEQ_RAW_BYTE_COUNT);
    g_inputs.b_steps_valid = (STATUS_OK == g_step_read_status);

    g_tempo_read_status = tempo_input_read(&g_inputs.tempo_raw);
    g_inputs.b_tempo_valid = (STATUS_OK == g_tempo_read_status);
}



/**
 * @brief  Brings the input snapshot up to date for this tick: records the
 *         elapsed time, and re-reads the input ports if a scan is due.
 * @param  elapsed_ticks  Ticks since the previous tick, 1 to 255.
 */
static void gather_inputs(uint8_t elapsed_ticks)
{
    g_inputs.elapsed_ticks = elapsed_ticks;

    if (elapsed_ticks >= g_ticks_until_scan)
    {
        scan_inputs();
        g_ticks_until_scan = INPUT_SCAN_PERIOD_TICKS;
    }
    else
    {
        g_ticks_until_scan = (uint8_t)(g_ticks_until_scan - elapsed_ticks);
    }
}



/**
 * @brief  Puts everything the tick carries from one pass to the next into
 *         its starting condition: nothing to apply, no inputs read yet, and
 *         a scan due on the first tick.
 */
static void reset_tick_state(void)
{
    seq_init(&g_seq_state);

    for (uint8_t i = 0U; i < SEQ_RAW_BYTE_COUNT; i++)
    {
        g_inputs.raw_steps[i] = 0U;
    }
    g_inputs.b_steps_valid = false;
    g_inputs.tempo_raw     = 0U;
    g_inputs.b_tempo_valid = false;
    g_inputs.elapsed_ticks = 0U;

    g_outputs.b_step_started = false;
    g_outputs.step           = 0U;
    g_outputs.note           = 0U;
    g_outputs.b_note_valid   = false;

    g_step_read_status  = STATUS_OK;
    g_tempo_read_status = STATUS_OK;
    g_ticks_until_scan  = 0U;
}



port_status_t app_init(void)
{
    const port_status_t status = platform_init();

    if (STATUS_OK != status)
    {
        // The main loop will not run, so nothing else would send this
        log_error(LOG_ERROR_INIT, status);
        log_flush();
    }
    reset_tick_state();

    return status;
}



void app_run_once(void)
{
    const uint8_t elapsed_ticks = timebase_elapsed_ticks();

    if (elapsed_ticks > 0U)
    {
        // A tick boundary. Outputs go first so that they change at the same
        // point in every tick, however long the rest of the pass takes.
        apply_outputs();
        gather_inputs(elapsed_ticks);
        seq_tick(&g_seq_state, &g_inputs, &g_outputs);
    }

    log_poll();
}
