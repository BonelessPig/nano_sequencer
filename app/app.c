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
#include "bank.h"
#include "cal.h"
#include "cal_record.h"
#include "errlog.h"
#include "midi_out.h"
#include "note_map.h"
#include "panel.h"
#include "pitch_cal.h"
#include "seq.h"
#include "transport.h"
#include "breadcrumb_port.h"
#include "clock_input_port.h"
#include "clock_output_port.h"
#include "cv_output_port.h"
#include "gate_output_port.h"
#include "led_output_port.h"
#include "level_output_port.h"
#include "log_port.h"
#include "midi_output_port.h"
#include "mod_input_port.h"
#include "nv_port.h"
#include "platform_port.h"
#include "reset_cause_port.h"
#include "reset_input_port.h"
#include "run_input_port.h"
#include "step_input_port.h"
#include "pot_input_port.h"
#include "timebase_port.h"
#include "watchdog_port.h"

// The panel and the tempo control are read once every this many ticks
// (125 times a second), not on every tick
#define INPUT_SCAN_PERIOD_TICKS (8U)

// The board has no gate length control yet: a note is held for half its step
#define GATE_LENGTH_PULSES (SEQ_PULSES_PER_STEP / 2U)

// What each slot of the amount bank sets, by the position of the selector
// that chooses it. The tempo is position 0, which is what a board with no
// selector wired reads, so that its pot is still the tempo
#define SLOT_TEMPO       (0U)
#define SLOT_TRANSPOSE   (1U) // How far the LFO moves a note in the scale
#define SLOT_GATE_LENGTH (2U) // How far it moves the gate length
#define SLOT_CHANCE      (3U) // How often it makes a step drop out

// The pitch CV is nominally 1 V to the octave from 0 V at the lowest pitch.
// That is the ideal calibration table, used until a measured one is stored,
// and what calibration mode trims each point from
#define CV_MILLIVOLTS_PER_OCTAVE (1000U)

// Where the calibration record is kept in the non-volatile storage
#define CAL_RECORD_ADDRESS (0U)

// Where the crash log is kept there, clear of the calibration record
#define ERRLOG_ADDRESS (16U)

// The places the breadcrumb marks, so that a crash log record says where the
// firmware was when the watchdog reset it. The numbers are stored and
// logged: never renumber them, and give a new place a new number
#define PLACE_STARTUP (1U) // After start-up, before the first tick
#define PLACE_APPLY   (2U) // Applying the outputs of the tick before
#define PLACE_GATHER  (3U) // Reading the inputs
#define PLACE_CORE    (4U) // In the core
#define PLACE_IDLE    (5U) // The tick finished: waiting for the next

static seq_state_t   g_seq_state; // Sequencer state carried between ticks
static seq_inputs_t  g_inputs;    // Latest snapshot of the inputs
static seq_outputs_t g_outputs;   // What the last tick computed; applied at the next

static cal_state_t   g_cal_state;   // Calibration mode's state carried between ticks
static cal_outputs_t g_cal_outputs; // What it computed on the last tick

static midi_out_state_t g_midi_state; // MIDI out's state carried between ticks
static midi_out_bytes_t g_midi_bytes; // What it computed on the last tick
static uint8_t          g_midi_room = 0U; // Bytes the MIDI output could take, as last read

static bank_state_t g_bank;                // The amounts the pot sets, one a slot
static uint16_t     g_pot_raw     = 0U;    // The pot's position, as last read
static bool         g_b_pot_valid = false; // false if it could not be read
static uint8_t      g_level       = 0U;    // The level light, as the last tick left it

static port_status_t g_step_read_status  = STATUS_OK; // Status of the last step input read
static port_status_t g_tempo_read_status = STATUS_OK; // Status of the last pot read
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
 *         given next; they show at the next input scan. The level light
 *         follows them. The MIDI bytes are
 *         queued after them, to be sent as the loop goes round. Then saves the
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
    level_output_write(g_level);
    midi_output_write(g_midi_bytes.bytes, g_midi_bytes.count);

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
 * @brief  Reads the panel and the LFO into the snapshot and the pot
 *         beside it; the panel read brings the mode switches and the
 *         selector with it. A failed read clears that input's valid flag
 *         and leaves its data as it was.
 */
static void scan_inputs(void)
{
    g_step_read_status = step_input_read(g_inputs.raw_steps, (uint8_t)PANEL_RAW_BYTE_COUNT);
    g_inputs.b_steps_valid = (STATUS_OK == g_step_read_status);

    g_tempo_read_status = pot_input_read(&g_pot_raw);
    g_b_pot_valid       = (STATUS_OK == g_tempo_read_status);

    g_inputs.mod.b_lfo_valid = (STATUS_OK == mod_input_read(&g_inputs.mod.lfo));
}



/**
 * @brief  Brings the input snapshot up to date for this tick: records the
 *         elapsed time, reads the transport inputs and the room the MIDI
 *         output has, which are read on every tick, and re-reads the panel
 *         and the pot if a scan is due.
 * @param  elapsed_ticks  Ticks since the previous tick, 1 to 255.
 */
static void gather_inputs(uint8_t elapsed_ticks)
{
    g_inputs.elapsed_ticks = elapsed_ticks;
    g_inputs.b_clock_edge  = clock_input_edge();
    g_inputs.b_reset       = reset_input_edge();
    g_inputs.b_run_button  = run_input_read();
    g_midi_room            = midi_output_room();

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
 * @brief  Works out the MIDI bytes for what the sequencer did on this tick,
 *         no more of them than the MIDI output had room for when it was
 *         last asked.
 */
static void run_midi_out(void)
{
    midi_out_inputs_t midi_in;

    midi_in.b_running      = (TRANSPORT_STOPPED != g_outputs.transport);
    midi_in.b_external     = (TRANSPORT_EXTERNAL == g_outputs.transport);
    midi_in.b_note_started = g_outputs.b_step_started && g_outputs.b_note_valid
                             && (!g_outputs.b_rest);
    midi_in.semitone       = g_outputs.semitone;
    midi_in.b_gate         = g_outputs.b_gate;
    midi_in.pulses         = g_outputs.pulses;
    midi_in.room           = g_midi_room;

    midi_out_tick(&g_midi_state, &midi_in, &g_midi_bytes);
}



/**
 * @brief  Lets the pot set the amount the selector chooses, and hands the
 *         sequencer the ones it uses: the tempo, and how much of the LFO
 *         goes to each thing it moves. The bank always has a tempo to
 *         give, so it is always valid. The level light is given the amount
 *         the selector is on.
 */
static void run_bank(void)
{
    bank_inputs_t bank_in;

    bank_in.selected        = panel_selector_value(g_inputs.raw_steps);
    bank_in.reading         = g_pot_raw;
    bank_in.b_reading_valid = g_b_pot_valid;
    bank_tick(&g_bank, &bank_in);

    g_inputs.tempo_raw     = bank_amount(&g_bank, (uint8_t)SLOT_TEMPO);
    g_inputs.b_tempo_valid = true;
    g_inputs.mod.transpose   = bank_amount(&g_bank, (uint8_t)SLOT_TRANSPOSE);
    g_inputs.mod.gate_length = bank_amount(&g_bank, (uint8_t)SLOT_GATE_LENGTH);
    g_inputs.mod.chance      = bank_amount(&g_bank, (uint8_t)SLOT_CHANCE);
    g_level                  = bank_level(&g_bank);
}



/**
 * @brief  Runs the core for this tick. Calibration goes first: if the panel
 *         asked for calibration mode at start-up, it gives the pitch CV and
 *         the gate, with the pot as it reads, and nothing is sequenced, and
 *         no MIDI is sent; otherwise the bank takes the pot, the sequencer
 *         runs, and MIDI out after it.
 */
static void run_core(void)
{
    cal_inputs_t cal_in;

    cal_in.p_raw_steps   = g_inputs.raw_steps;
    cal_in.b_steps_valid = g_inputs.b_steps_valid;
    cal_in.tempo_raw     = g_pot_raw;
    cal_in.b_tempo_valid = g_b_pot_valid;
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
        run_bank();
        seq_tick(&g_seq_state, &g_inputs, &g_outputs);
        run_midi_out();
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
    g_inputs.mod.lfo         = 0U;
    g_inputs.mod.b_lfo_valid = false;
    g_inputs.mod.transpose   = 0U;
    g_inputs.mod.gate_length = 0U;
    g_inputs.mod.chance      = 0U;
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
    g_outputs.pulses              = 0U;

    cal_init(&g_cal_state);
    g_cal_outputs.b_active = false;
    g_cal_outputs.cv       = 0U;
    g_cal_outputs.b_gate   = false;
    g_cal_outputs.b_stored = false;
    g_cal_outputs.point    = 0U;

    bank_init(&g_bank, (uint8_t)SLOT_TEMPO);
    g_pot_raw     = 0U;
    g_b_pot_valid = false;
    g_level       = 0U;

    midi_out_init(&g_midi_state);
    g_midi_bytes.count = 0U;
    g_midi_room        = 0U;

    g_step_read_status  = STATUS_OK;
    g_tempo_read_status = STATUS_OK;
    g_ticks_until_scan  = 0U;
}



/**
 * @brief  Loads the stored pitch calibration over the ideal table, if there
 *         is a valid record. A read that fails is logged and leaves the
 *         ideal table, as does anything that is not a record (storage never
 *         written, say).
 * @return true if a stored calibration is now in use.
 */
static bool load_calibration(void)
{
    uint8_t             record[CAL_RECORD_SIZE];
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

    return b_calibrated;
}



/**
 * @brief  Brings the crash log up to date with this start-up and logs its
 *         newest record, if it has one. If the watchdog caused the reset,
 *         the place the breadcrumb was left at goes into the log, and the
 *         write of the one record that changed is started. A read or a
 *         write that fails is logged, and the log is then left as it is.
 * @param  reset_cause  What reset the processor, as reset_cause_read() gave
 *                      it.
 */
static void update_crash_log(uint8_t reset_cause)
{
    uint8_t        records[ERRLOG_SIZE];
    uint8_t        offset  = 0U;
    const uint16_t address = ERRLOG_ADDRESS;
    errlog_entry_t newest;
    port_status_t  status  = nv_read(address, records, (uint8_t)ERRLOG_SIZE);

    if (STATUS_OK != status)
    {
        log_error(LOG_ERROR_CRASH_READ, status);
    }
    else if (errlog_add(records, reset_cause, breadcrumb_read(), &offset))
    {
        status = nv_write((uint16_t)(address + offset), &records[offset],
                          (uint8_t)ERRLOG_RECORD_SIZE);
        if (STATUS_OK != status)
        {
            log_error(LOG_ERROR_CRASH_WRITE, status);
        }
    }
    else
    {
        // Not a crash, or one the log has counted as often as it can
    }

    if ((STATUS_OK == status) && errlog_newest(records, &newest))
    {
        log_crash(newest.number, newest.place, newest.count);
    }
}



/**
 * @brief  The part of start-up that follows a platform brought up: reads
 *         what reset the processor and the stored calibration and logs
 *         both, then sees to the crash log. The breadcrumb is read for that
 *         before it is given its first place of this run.
 */
static void start_up(void)
{
    const uint8_t reset_cause  = reset_cause_read();
    const bool    b_calibrated = load_calibration();

    log_startup(reset_cause, b_calibrated);
    update_crash_log(reset_cause);
    breadcrumb_write(PLACE_STARTUP);
}



port_status_t app_init(void)
{
    const port_status_t status = platform_init();

    reset_tick_state();

    if (STATUS_OK == status)
    {
        start_up();
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
        // Each part is marked as it begins, for the crash log
        breadcrumb_write(PLACE_APPLY);
        apply_outputs();

        // Kicked only here, so that the watchdog catches a tick that has
        // stopped coming as well as a loop that has stopped going round
        watchdog_kick();
        breadcrumb_write(PLACE_GATHER);
        gather_inputs(elapsed_ticks);
        breadcrumb_write(PLACE_CORE);
        run_core();
        breadcrumb_write(PLACE_IDLE);
    }

    log_poll();
    midi_output_poll();
    nv_poll();
}
