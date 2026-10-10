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
#include "address.h"
#include "note_map.h"
#include "panel.h"
#include "pitch_cal.h"
#include "seq.h"
#include "clock_output_port.h"
#include "cv_output_port.h"
#include "gate_output_port.h"
#include "log_port.h"
#include "platform_port.h"
#include "step_input_port.h"
#include "tempo_input_port.h"
#include "timebase_port.h"

// The panel and the tempo control are read once every this many ticks
// (125 times a second), not on every tick
#define INPUT_SCAN_PERIOD_TICKS (8U)

// The board has no gate length control yet: a note is held for half its step
#define GATE_LENGTH_PULSES (SEQ_PULSES_PER_STEP / 2U)

// The pitch CV is not calibrated yet: the table is the ideal one, 1 V to the
// octave from 0 V at the lowest pitch
#define CV_MILLIVOLTS_PER_OCTAVE (1000U)

static seq_state_t   g_seq_state; // Sequencer state carried between ticks
static seq_inputs_t  g_inputs;    // Latest snapshot of the inputs
static seq_outputs_t g_outputs;   // What the last tick computed; applied at the next

static port_status_t g_step_read_status  = STATUS_OK; // Status of the last step input read
static port_status_t g_tempo_read_status = STATUS_OK; // Status of the last tempo input read
static uint8_t       g_ticks_until_scan  = 0U;        // Ticks before the inputs are read again



/**
 * @brief  Logs the step that began: its pitch, that it is a rest, or the
 *         failed step read that left it unknown.
 */
static void log_step(void)
{
    if (!g_outputs.b_note_valid)
    {
        log_error(LOG_ERROR_STEP_READ, g_step_read_status);
    }
    else if (g_outputs.b_rest)
    {
        log_step_rest(g_outputs.step);
    }
    else
    {
        log_step_note(g_outputs.step, g_outputs.semitone);
    }
}



/**
 * @brief  Acts on what the previous tick computed. The pitch CV, the gate
 *         and the clock output are set first, on every tick, so that their
 *         edges are not held up by the log; the pitch goes ahead of the gate
 *         so that it is in place when the note starts. Then, when a step
 *         began, logs it (or the failed step read), and with it a failed
 *         tempo read and a failed write of the pitch.
 */
static void apply_outputs(void)
{
    const port_status_t cv_write_status = cv_output_write(g_outputs.cv);

    gate_output_write(g_outputs.b_gate);
    clock_output_write(g_outputs.b_clock_out);

    if (g_outputs.b_step_started)
    {
        log_step();

        if (STATUS_OK != g_tempo_read_status)
        {
            log_error(LOG_ERROR_TEMPO_READ, g_tempo_read_status);
        }
        if (STATUS_OK != cv_write_status)
        {
            log_error(LOG_ERROR_CV_WRITE, cv_write_status);
        }
    }
}



/**
 * @brief  Reads the step and tempo input ports into the snapshot. A failed
 *         read clears that input's valid flag and leaves its data as it was.
 */
static void scan_inputs(void)
{
    g_step_read_status = step_input_read(g_inputs.raw_steps, (uint8_t)PANEL_RAW_BYTE_COUNT);
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
 * @brief  Sets the controls the board has no hardware for yet to fixed
 *         values: all sixteen steps, forward, looping, never reset, the
 *         chromatic scale from the lowest pitch, a gate of half a step, and
 *         the ideal pitch calibration table.
 */
static void set_fixed_controls(void)
{
    g_inputs.address.direction  = ADDRESS_FORWARD;
    g_inputs.address.first_step = 0U;
    g_inputs.address.last_step  = (uint8_t)(PANEL_STEP_COUNT - 1U);
    g_inputs.address.b_one_shot = false;
    g_inputs.note_map.scale     = NOTE_MAP_SCALE_CHROMATIC;
    g_inputs.note_map.root      = 0U;
    g_inputs.gate.length        = (uint8_t)GATE_LENGTH_PULSES;
    g_inputs.b_reset            = false;

    for (uint8_t point = 0U; point < PITCH_CAL_POINT_COUNT; point++)
    {
        g_inputs.pitch_cal.cv[point] = (uint16_t)(point * CV_MILLIVOLTS_PER_OCTAVE);
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

    for (uint8_t i = 0U; i < PANEL_RAW_BYTE_COUNT; i++)
    {
        g_inputs.raw_steps[i] = 0U;
    }
    g_inputs.b_steps_valid = false;
    g_inputs.tempo_raw     = 0U;
    g_inputs.b_tempo_valid = false;
    g_inputs.elapsed_ticks = 0U;
    set_fixed_controls();

    g_outputs.b_step_started = false;
    g_outputs.step           = 0U;
    g_outputs.b_note_valid   = false;
    g_outputs.b_rest         = false;
    g_outputs.semitone       = 0U;
    g_outputs.b_gate         = false;
    g_outputs.b_clock_out    = false;
    g_outputs.cv             = 0U;

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
