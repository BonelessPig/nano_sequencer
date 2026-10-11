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
#include "cal.h"
#include "cal_record.h"
#include "note_map.h"
#include "panel.h"
#include "pitch_cal.h"
#include "seq.h"
#include "transport.h"
#include "clock_input_port.h"
#include "clock_output_port.h"
#include "cv_output_port.h"
#include "gate_output_port.h"
#include "led_output_port.h"
#include "log_port.h"
#include "nv_port.h"
#include "platform_port.h"
#include "reset_cause_port.h"
#include "reset_input_port.h"
#include "run_input_port.h"
#include "step_input_port.h"
#include "sync_input_port.h"
#include "tempo_input_port.h"
#include "timebase_port.h"
#include "watchdog_port.h"

// The panel and the tempo control are read once every this many ticks
// (125 times a second), not on every tick
#define INPUT_SCAN_PERIOD_TICKS (8U)

// The board has no gate length control yet: a note is held for half its step
#define GATE_LENGTH_PULSES (SEQ_PULSES_PER_STEP / 2U)

// The pitch CV is nominally 1 V to the octave from 0 V at the lowest pitch.
// That is the ideal calibration table, used until a measured one is stored,
// and what calibration mode trims each point from
#define CV_MILLIVOLTS_PER_OCTAVE (1000U)

// Where the calibration record is kept in the non-volatile storage
#define CAL_RECORD_ADDRESS (0U)

static seq_state_t   g_seq_state; // Sequencer state carried between ticks
static seq_inputs_t  g_inputs;    // Latest snapshot of the inputs
static seq_outputs_t g_outputs;   // What the last tick computed; applied at the next

static cal_state_t   g_cal_state;   // Calibration mode's state carried between ticks
static cal_outputs_t g_cal_outputs; // What it computed on the last tick

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
 * @brief  Saves the calibration table after calibration mode has stored a
 *         point in it: starts the write of the whole record and logs the
 *         point, or the failure to start.
 */
static void save_calibration(void)
{
    uint8_t       record[CAL_RECORD_SIZE];
    port_status_t status;

    cal_record_encode(&g_inputs.pitch_cal, record);
    status = nv_write(CAL_RECORD_ADDRESS, record, (uint8_t)CAL_RECORD_SIZE);

    if (STATUS_OK == status)
    {
        log_cal_point(g_cal_outputs.point, g_cal_outputs.cv);
    }
    else
    {
        log_error(LOG_ERROR_NV_WRITE, status);
    }
}



/**
 * @brief  Acts on what the previous tick computed. The pitch CV, the gate
 *         and the clock output are set first, on every tick, so that their
 *         edges are not held up by the log; the pitch goes ahead of the gate
 *         so that it is in place when the note starts. The step lights are
 *         given next; they show at the next input scan. Then saves the
 *         calibration if a point of it was stored, logs the transport if it
 *         changed, and, when a step began, logs it (or the failed step
 *         read), and with it a failed tempo read and a failed write of the
 *         pitch.
 */
static void apply_outputs(void)
{
    const port_status_t cv_write_status = cv_output_write(g_outputs.cv);

    gate_output_write(g_outputs.b_gate);
    clock_output_write(g_outputs.b_clock_out);
    led_output_write(g_outputs.leds);

    if (g_cal_outputs.b_stored)
    {
        save_calibration();
    }

    if (g_outputs.b_transport_changed)
    {
        log_transport((uint8_t)g_outputs.transport);
    }

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
 * @brief  Reads the step and tempo input ports into the snapshot; the
 *         step read brings the mode switches with it. A failed read clears
 *         that input's valid flag and leaves its data as it was.
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
 *         elapsed time, reads the transport inputs, which are read on every
 *         tick, and re-reads the panel and the tempo if a scan is due.
 * @param  elapsed_ticks  Ticks since the previous tick, 1 to 255.
 */
static void gather_inputs(uint8_t elapsed_ticks)
{
    g_inputs.elapsed_ticks = elapsed_ticks;
    g_inputs.b_clock_edge  = clock_input_edge();
    g_inputs.b_reset       = reset_input_edge();
    g_inputs.b_run_button  = run_input_read();
    g_inputs.b_sync_switch = sync_input_read();

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
 * @brief  Runs the core for this tick. Calibration goes first: if the panel
 *         asked for calibration mode at start-up, it gives the pitch CV and
 *         the gate and nothing is sequenced; otherwise the sequencer runs.
 */
static void run_core(void)
{
    cal_inputs_t cal_in;

    cal_in.p_raw_steps   = g_inputs.raw_steps;
    cal_in.b_steps_valid = g_inputs.b_steps_valid;
    cal_in.tempo_raw     = g_inputs.tempo_raw;
    cal_in.b_tempo_valid = g_inputs.b_tempo_valid;
    cal_in.elapsed_ticks = g_inputs.elapsed_ticks;
    cal_in.cv_per_octave = CV_MILLIVOLTS_PER_OCTAVE;

    cal_tick(&g_cal_state, &cal_in, &g_inputs.pitch_cal, &g_cal_outputs);

    if (g_cal_outputs.b_active)
    {
        // The rest of the outputs stay as init left them: no step, clock low
        g_outputs.cv     = g_cal_outputs.cv;
        g_outputs.b_gate = g_cal_outputs.b_gate;
    }
    else
    {
        seq_tick(&g_seq_state, &g_inputs, &g_outputs);
    }
}



/**
 * @brief  Sets the controls the board has no hardware for yet to fixed
 *         values: all sixteen steps, forward, looping, the chromatic scale
 *         from the lowest pitch, a gate of half a step, and
 *         the ideal pitch calibration table, which stays unless a stored
 *         one is loaded over it.
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
    g_inputs.b_reset       = false;
    g_inputs.b_clock_edge  = false;
    g_inputs.b_run_button  = false;
    g_inputs.b_sync_switch = false;
    set_fixed_controls();

    g_outputs.b_step_started = false;
    g_outputs.step           = 0U;
    g_outputs.b_note_valid   = false;
    g_outputs.b_rest         = false;
    g_outputs.semitone       = 0U;
    g_outputs.b_gate         = false;
    g_outputs.b_clock_out    = false;
    g_outputs.cv             = 0U;
    g_outputs.leds           = 0U;
    g_outputs.transport           = TRANSPORT_INTERNAL;
    g_outputs.b_transport_changed = false;

    cal_init(&g_cal_state);
    g_cal_outputs.b_active = false;
    g_cal_outputs.cv       = 0U;
    g_cal_outputs.b_gate   = false;
    g_cal_outputs.b_stored = false;
    g_cal_outputs.point    = 0U;

    g_step_read_status  = STATUS_OK;
    g_tempo_read_status = STATUS_OK;
    g_ticks_until_scan  = 0U;
}



/**
 * @brief  Loads the stored pitch calibration over the ideal table, if there
 *         is a valid record, and logs how the firmware started: what reset
 *         it, and whether a stored calibration is in use. A read that fails
 *         is logged and leaves the ideal table, as does anything that is not
 *         a record (storage never written, say).
 */
static void load_calibration(void)
{
    uint8_t             record[CAL_RECORD_SIZE];
    const uint8_t       reset_cause  = reset_cause_read();
    const port_status_t status       =
        nv_read(CAL_RECORD_ADDRESS, record, (uint8_t)CAL_RECORD_SIZE);
    bool                b_calibrated = false;

    if (STATUS_OK == status)
    {
        b_calibrated = cal_record_decode(record, &g_inputs.pitch_cal);
    }
    else
    {
        log_error(LOG_ERROR_NV_READ, status);
    }

    log_startup(reset_cause, b_calibrated);
}



port_status_t app_init(void)
{
    const port_status_t status = platform_init();

    reset_tick_state();

    if (STATUS_OK == status)
    {
        load_calibration();
    }
    else
    {
        // The main loop will not run, so nothing else would send this
        log_error(LOG_ERROR_INIT, status);
        log_flush();
    }

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

        // Kicked only here, so that the watchdog catches a tick that has
        // stopped coming as well as a loop that has stopped going round
        watchdog_kick();
        gather_inputs(elapsed_ticks);
        run_core();
    }

    log_poll();
    nv_poll();
}
