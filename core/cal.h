#ifndef CAL_H
#define CAL_H
/**
 * @file   cal.h
 * @brief  Calibration mode: the panel and the tempo control used to set the
 *         pitch calibration table against a voltmeter, one octave at a time.
 *         Pure logic with no hardware access; compiles unchanged for the MCU
 *         and for a PC.
 * @author BonelessPig
 *
 * The mode is chosen once, on the first tick after cal_init(), and lasts
 * until the next cal_init() (on the board, until the power is cycled). It is
 * entered if the last four steps of the panel then read 10, 5, 10, 5: their
 * switches set alternately, like a chequerboard. Any other panel, or one that
 * could not be read, plays as normal, and this module then does nothing.
 *
 * In the mode nothing is sequenced. The pitch CV is held at one point of the
 * table, so that it can be measured:
 *   - Step 0's switches choose the point, 0 to 4 (a higher value is point 4).
 *     Point n is meant to come out at n octaves above the lowest pitch.
 *   - The tempo control trims the CV value: its middle gives the nominal
 *     value of the point (n times cv_per_octave), and its travel covers 64
 *     values below that to 63 above, in steps of one.
 *   - Switching on the lowest switch of step 15 stores the value now given as
 *     that point of the table. It is on when the mode is entered, so it is
 *     switched off and on again.
 *   - The gate is high, so that something patched to it sounds, and goes low
 *     for a quarter of a second after each store to show that it happened.
 * Only a stored point changes the table; the others keep what they held.
 *
 * @copyright Copyright (c) 2026
 *
 */
#include <stdbool.h>
#include <stdint.h>
#include "pitch_cal.h"

/**
 * @brief State calibration carries from one tick to the next.
 */
typedef struct
{
    bool     b_decided;      // false until the first tick has chosen the mode
    bool     b_active;       // true in calibration mode
    bool     b_store_was_on; // The store switch as it was last read
    uint8_t  point;          // Table entry being set, 0 to 4
    uint16_t trim_raw;       // Most recent valid tempo reading, 0 to 1023
    uint8_t  confirm_ticks;  // Ticks the gate has still to stay low after a store
} cal_state_t;

/**
 * @brief One snapshot of the controls, gathered before a tick.
 */
typedef struct
{
    // The panel's step bits, packed as panel.h describes. A null pointer is
    // treated as steps that could not be read
    const uint8_t *p_raw_steps;
    bool           b_steps_valid; // false if the steps could not be read
    uint16_t       tempo_raw;     // Tempo control position, 0 to 1023. A
                                  // higher reading is treated as 1023
    bool           b_tempo_valid; // false if tempo_raw could not be read
    uint8_t        elapsed_ticks; // Timebase ticks since the previous tick
    uint16_t       cv_per_octave; // Nominal CV value of one octave, 0 to
                                  // PITCH_CAL_CV_MAX; a higher value is
                                  // treated as PITCH_CAL_CV_MAX
} cal_inputs_t;

/**
 * @brief What calibration wants done after a tick. Every field is false or 0
 *        outside calibration mode.
 */
typedef struct
{
    bool     b_active; // true in calibration mode: the fields below are to be
                       // used and nothing is to be sequenced. Given on every
                       // tick
    uint16_t cv;       // Pitch CV value after this tick, 0 to
                       // PITCH_CAL_CV_MAX. Given on every tick
    bool     b_gate;   // Level of the gate after this tick. Given on every
                       // tick
    bool     b_stored; // true on the tick a point is stored: the table has
                       // changed and is to be saved
    uint8_t  point;    // The point stored, 0 to 4, on that tick; 0 otherwise.
                       // Its new value is cv
} cal_outputs_t;

/**
 * @brief  Puts the state into its power-on condition: the mode is chosen
 *         afresh on the next tick.
 * @param  p_state  State to initialize. Ignored if null.
 */
void cal_init(cal_state_t *p_state);

/**
 * @brief  Runs one tick. The first after cal_init() chooses the mode from
 *         the panel as it then reads; in calibration mode every tick gives
 *         the pitch CV and the gate, and stores a point when asked (see the
 *         top of this file). Reads *p_state and *p_in, writes *p_state,
 *         *p_out and, on a store, one entry of *p_table, and has no other
 *         effect.
 *
 *         When the steps cannot be read the point stays as it was and
 *         nothing is stored; when the tempo control cannot be read the trim
 *         stays as it was, and it is at its middle until the first reading.
 * @param  p_state  State from cal_init() or the previous tick; updated.
 * @param  p_in     Inputs for this tick.
 * @param  p_table  The calibration table in use. Entry p_out->point is set
 *                  to p_out->cv on a tick where p_out->b_stored is true.
 * @param  p_out    Receives the outputs; every field is written.
 * @note   Does nothing if any pointer is null.
 */
void cal_tick(cal_state_t *p_state, const cal_inputs_t *p_in,
              pitch_cal_config_t *p_table, cal_outputs_t *p_out);

#endif /* CAL_H */
