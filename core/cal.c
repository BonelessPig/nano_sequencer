/**
 * @file   cal.c
 * @brief  Calibration mode implementation. Pure logic: no hardware access.
 * @author BonelessPig
 *
 * @copyright Copyright (c) 2026
 *
 */
#include "cal.h"
#include <stddef.h>
#include "panel.h"

// The mode is entered when the last four steps read 10, 5, 10, 5
#define ENTRY_FIRST_STEP  (12U)
#define ENTRY_EVEN_VALUE  (0x0AU) // What steps 12 and 14 must read
#define ENTRY_ODD_VALUE   (0x05U) // What steps 13 and 15 must read

#define POINT_STEP        (0U)    // The step whose value chooses the point
#define LAST_POINT        (PITCH_CAL_POINT_COUNT - 1U)
#define STORE_STEP        (15U)   // The step with the store switch
#define STORE_SWITCH_MASK (0x01U) // Its lowest switch

// The tempo reading as a trim: 8 counts of it to one CV value, and its
// middle (512) for no trim, which is 64 values up from its lowest
#define TRIM_RAW_MAX      (1023U)
#define TRIM_RAW_CENTRE   (512U)
#define TRIM_RAW_PER_CV   (8U)
#define TRIM_CV_CENTRE    (TRIM_RAW_CENTRE / TRIM_RAW_PER_CV)

#define CONFIRM_TICKS     (250U)  // How long the gate stays low after a store



/**
 * @brief  Tells whether the panel asks for calibration mode.
 * @param  p_raw_steps  The panel's step bits; not null.
 * @return true if the last four steps read 10, 5, 10, 5.
 */
static bool is_entry_pattern(const uint8_t *p_raw_steps)
{
    bool b_entry = true;

    for (uint8_t step = ENTRY_FIRST_STEP; step < PANEL_STEP_COUNT; step++)
    {
        const uint8_t expected =
            (0U == (step % 2U)) ? (uint8_t)ENTRY_EVEN_VALUE : (uint8_t)ENTRY_ODD_VALUE;

        if (expected != panel_step_value(p_raw_steps, step))
        {
            b_entry = false;
        }
    }

    return b_entry;
}



/**
 * @brief  Works out the CV value for a point with the trim applied.
 * @param  point          Table entry, 0 to LAST_POINT.
 * @param  trim_raw       Tempo reading, 0 to TRIM_RAW_MAX.
 * @param  cv_per_octave  Nominal CV value of one octave.
 * @return The value, 0 to PITCH_CAL_CV_MAX.
 */
static uint16_t trimmed_cv(uint8_t point, uint16_t trim_raw, uint16_t cv_per_octave)
{
    uint16_t per_octave = cv_per_octave;
    uint16_t raised;
    uint16_t cv = 0U;

    if (per_octave > PITCH_CAL_CV_MAX)
    {
        per_octave = PITCH_CAL_CV_MAX;
    }

    // The nominal value plus the trim counted up from its lowest. At most
    // 4 * 4095 + 127 = 16507, which fits in 16 bits
    raised = (uint16_t)((uint16_t)(point * per_octave) +
                        (uint16_t)(trim_raw / TRIM_RAW_PER_CV));

    if (raised > TRIM_CV_CENTRE)
    {
        cv = (uint16_t)(raised - TRIM_CV_CENTRE);
    }
    if (cv > PITCH_CAL_CV_MAX)
    {
        cv = PITCH_CAL_CV_MAX;
    }

    return cv;
}



/**
 * @brief  Reads the point and the store switch from the panel.
 * @param  p_state      Calibration state; the point and the switch are updated.
 * @param  p_raw_steps  The panel's step bits; not null.
 * @return true if the store switch has just been switched on.
 */
static bool read_panel(cal_state_t *p_state, const uint8_t *p_raw_steps)
{
    const uint8_t value      = panel_step_value(p_raw_steps, POINT_STEP);
    const uint8_t store_step = panel_step_value(p_raw_steps, STORE_STEP);
    const bool    b_store_on = (0U != (store_step & STORE_SWITCH_MASK));
    const bool    b_store    = b_store_on && !p_state->b_store_was_on;

    p_state->point          = (value > LAST_POINT) ? (uint8_t)LAST_POINT : value;
    p_state->b_store_was_on = b_store_on;

    return b_store;
}



/**
 * @brief  Runs one tick of calibration mode.
 * @param  p_state        Calibration state; updated.
 * @param  p_in           Inputs for this tick.
 * @param  b_steps_valid  true if p_in->p_raw_steps can be used.
 * @param  p_table        The calibration table; one entry set on a store.
 * @param  p_out          Outputs, already cleared; filled in.
 */
static void run_mode(cal_state_t *p_state, const cal_inputs_t *p_in, bool b_steps_valid,
                     pitch_cal_config_t *p_table, cal_outputs_t *p_out)
{
    bool b_store = false;

    if (p_in->elapsed_ticks >= p_state->confirm_ticks)
    {
        p_state->confirm_ticks = 0U;
    }
    else
    {
        p_state->confirm_ticks = (uint8_t)(p_state->confirm_ticks - p_in->elapsed_ticks);
    }

    // An invalid tempo reading leaves the last valid one in place
    if (p_in->b_tempo_valid)
    {
        p_state->trim_raw =
            (p_in->tempo_raw > TRIM_RAW_MAX) ? (uint16_t)TRIM_RAW_MAX : p_in->tempo_raw;
    }

    if (b_steps_valid)
    {
        b_store = read_panel(p_state, p_in->p_raw_steps);
    }

    p_out->b_active = true;
    p_out->cv       = trimmed_cv(p_state->point, p_state->trim_raw, p_in->cv_per_octave);

    if (b_store)
    {
        p_table->cv[p_state->point] = p_out->cv;
        p_state->confirm_ticks      = CONFIRM_TICKS;
        p_out->b_stored             = true;
        p_out->point                = p_state->point;
    }

    p_out->b_gate = (0U == p_state->confirm_ticks);
}



void cal_init(cal_state_t *p_state)
{
    if (NULL != p_state)
    {
        p_state->b_decided      = false;
        p_state->b_active       = false;
        p_state->b_store_was_on = true; // As the entry pattern leaves it
        p_state->point          = 0U;
        p_state->trim_raw       = TRIM_RAW_CENTRE;
        p_state->confirm_ticks  = 0U;
    }
}



void cal_tick(cal_state_t *p_state, const cal_inputs_t *p_in,
              pitch_cal_config_t *p_table, cal_outputs_t *p_out)
{
    if ((NULL != p_state) && (NULL != p_in) && (NULL != p_table) && (NULL != p_out))
    {
        const bool b_steps_valid = p_in->b_steps_valid && (NULL != p_in->p_raw_steps);

        p_out->b_active = false;
        p_out->cv       = 0U;
        p_out->b_gate   = false;
        p_out->b_stored = false;
        p_out->point    = 0U;

        // The mode is chosen once; a panel that cannot be read plays as normal
        if (!p_state->b_decided)
        {
            p_state->b_decided = true;
            p_state->b_active  = b_steps_valid && is_entry_pattern(p_in->p_raw_steps);
        }

        if (p_state->b_active)
        {
            run_mode(p_state, p_in, b_steps_valid, p_table, p_out);
        }
    }
}
