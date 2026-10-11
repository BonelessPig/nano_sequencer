/**
 * @file   seq.c
 * @brief  Sequencer core implementation. Pure logic: no hardware access.
 * @author BonelessPig
 *
 * @copyright Copyright (c) 2026
 *
 */
#include "seq.h"
#include <stddef.h>
#include "engine.h"

// On the external clock the tempo reading is a ratio. Its range is cut into
// seven parts of this many units (the last a few short): three that divide
// the clock, one that is a step an edge, and three that multiply it
#define SYNC_RAW_PER_PART (147U)
#define SYNC_PART_UNITY   (3U)
#define SYNC_PART_MAX     (6U)

// The low three mode switches choose the engine, as a number. Every number
// with no engine of its own is the plain one
#define ENGINE_SELECT_MASK (0x07U)
#define ENGINE_PLAIN       (0U)
#define ENGINE_STAGE       (1U)
#define ENGINE_KLEE        (2U)
#define ENGINE_CHANCE      (3U)
#define ENGINE_CARTESIAN   (4U)
#define ENGINE_EUCLID      (5U)

// Where the random numbers start from at power-on; any value but 0 will do
#define PRNG_SEED (0xACE1U)



/**
 * @brief  Converts a tempo control reading to a tempo.
 * @param  tempo_raw  Tempo control position, 0 to 1023.
 * @return Beats per minute, SEQ_BPM_MIN to SEQ_BPM_MIN + 255.
 */
static uint16_t tempo_bpm(uint16_t tempo_raw)
{
    return (uint16_t)(SEQ_BPM_MIN + (tempo_raw / SEQ_TEMPO_RAW_PER_BPM));
}



/**
 * @brief  Converts a tempo control reading to a ratio to the external clock.
 * @param  tempo_raw  Tempo control position, 0 to 1023. Anything above is
 *                    taken as the top of the range.
 * @param  p_follow   Receives the ratio: divide 4, 3, 2 or 1 and multiply 1
 *                    over the lower half, divide 1 and multiply 2, 3 or 4
 *                    over the upper.
 */
static void sync_ratio(uint16_t tempo_raw, clock_follow_t *p_follow)
{
    uint16_t part = (uint16_t)(tempo_raw / SYNC_RAW_PER_PART);

    if (part > SYNC_PART_MAX)
    {
        part = (uint16_t)SYNC_PART_MAX;
    }

    p_follow->divide   = 1U;
    p_follow->multiply = 1U;
    if (part < SYNC_PART_UNITY)
    {
        p_follow->divide = (uint8_t)((SYNC_PART_UNITY + 1U) - part);
    }
    else
    {
        p_follow->multiply = (uint8_t)((part - SYNC_PART_UNITY) + 1U);
    }
}



/**
 * @brief  Moves the clock on and gives the pulses that fell due: at the
 *         tempo on the internal clock, in step with the edges on the
 *         external one.
 * @param  p_state        Sequencer state; the clock moves on.
 * @param  p_in           Inputs for this tick.
 * @param  mode           Which clock the pattern is running from.
 * @param  elapsed_ticks  Ticks to move the clock on by.
 * @return Pulses that fell due.
 */
static uint8_t pulses_due(seq_state_t *p_state, const seq_inputs_t *p_in,
                          transport_mode_t mode, uint8_t elapsed_ticks)
{
    uint8_t pulses;

    if (TRANSPORT_EXTERNAL == mode)
    {
        clock_follow_t follow;

        follow.b_edge         = p_in->b_clock_edge;
        follow.elapsed_ticks  = elapsed_ticks;
        follow.pulses_to_step = 0U;
        if (p_state->step_pulses < SEQ_PULSES_PER_STEP)
        {
            // At most 24, so it fits in a byte
            follow.pulses_to_step = (uint8_t)(SEQ_PULSES_PER_STEP - p_state->step_pulses);
        }
        sync_ratio(p_state->last_tempo_raw, &follow);
        pulses = clock_follow(&p_state->clock, &follow);
    }
    else
    {
        pulses = clock_advance(&p_state->clock, tempo_bpm(p_state->last_tempo_raw),
                               elapsed_ticks);
    }

    return pulses;
}



/**
 * @brief  Sends every engine back to the start of its pattern.
 * @param  p_state  Sequencer state; the choice of engine is kept.
 */
static void engines_init(seq_state_t *p_state)
{
    engine_plain_init(&p_state->plain);
    engine_stage_init(&p_state->stage);
    engine_klee_init(&p_state->klee);
    engine_chance_init(&p_state->chance);
    engine_cartesian_init(&p_state->cartesian);
    engine_euclid_init(&p_state->euclid);
}



/**
 * @brief  Asks the engine the mode switches choose for the step to play. An
 *         engine that was not the one to play the last step starts its
 *         pattern from the beginning.
 * @param  p_state  Sequencer state; the engine moves on.
 * @param  p_in     Inputs for this tick.
 * @param  p_step   Receives the step, if there is one.
 * @return true if there is a step to play.
 */
static bool engine_step(seq_state_t *p_state, const seq_inputs_t *p_in,
                        engine_step_t *p_step)
{
    engine_inputs_t engine_in;
    uint8_t         engine =
        (uint8_t)(panel_mode_value(p_in->raw_steps) & ENGINE_SELECT_MASK);
    bool            b_step;

    if (engine > ENGINE_EUCLID)
    {
        engine = (uint8_t)ENGINE_PLAIN;
    }
    if (engine != p_state->engine)
    {
        engines_init(p_state);
        p_state->engine = engine;
    }

    engine_in.p_raw_steps = p_in->raw_steps;
    engine_in.p_address   = &p_in->address;
    engine_in.p_prng      = &p_state->prng;

    switch (engine)
    {
        case ENGINE_STAGE:
            b_step = engine_stage_step(&p_state->stage, &engine_in, p_step);
            break;

        case ENGINE_KLEE:
            b_step = engine_klee_step(&p_state->klee, &engine_in, p_step);
            break;

        case ENGINE_CHANCE:
            b_step = engine_chance_step(&p_state->chance, &engine_in, p_step);
            break;

        case ENGINE_CARTESIAN:
            b_step = engine_cartesian_step(&p_state->cartesian, &engine_in, p_step);
            break;

        case ENGINE_EUCLID:
            b_step = engine_euclid_step(&p_state->euclid, &engine_in, p_step);
            break;

        default:
            b_step = engine_plain_step(&p_state->plain, &engine_in, p_step);
            break;
    }

    return b_step;
}



/**
 * @brief  Gives the step lights for a step that has just begun: the light
 *         of the step itself, or for the Klee engine the active stages of
 *         its register.
 * @param  p_state  Sequencer state, after the engine has given the step.
 * @param  step     The step it gave, 0 to PANEL_STEP_COUNT - 1.
 * @return One bit a light, bit n for step n.
 */
static uint16_t step_leds(const seq_state_t *p_state, uint8_t step)
{
    uint16_t leds = p_state->klee.stages;

    if (ENGINE_KLEE != p_state->engine)
    {
        leds = 1U;
        for (uint8_t i = 0U; i < step; i++)
        {
            leds = (uint16_t)(leds << 1U);
        }
    }

    return leds;
}



/**
 * @brief  Begins the step that is due, if the engine has one to play, and
 *         works out its pitch and, for a note, the pitch CV value.
 * @param  p_state  Sequencer state; the engine moves on.
 * @param  p_in     Inputs for this tick.
 * @param  p_out    Outputs, already cleared; filled in if a step begins.
 * @return true if a note begins that is held into the step after it.
 */
static bool begin_step(seq_state_t *p_state, const seq_inputs_t *p_in, seq_outputs_t *p_out)
{
    engine_step_t played;

    played.step  = 0U;
    played.note  = 0U;
    played.b_tie = false;

    // The step takes its turn whether or not its note is readable
    p_state->leds = 0U;
    if (engine_step(p_state, p_in, &played))
    {
        p_state->leds         = step_leds(p_state, played.step);
        p_out->b_step_started = true;
        p_out->step           = played.step;
        p_out->b_note_valid   = p_in->b_steps_valid;
        if (p_in->b_steps_valid)
        {
            p_out->b_rest = !note_map_semitone(&p_in->note_map, played.note,
                                               &p_out->semitone);
            if (!p_out->b_rest)
            {
                p_state->cv = pitch_cal_cv(&p_in->pitch_cal, p_out->semitone);
            }
        }
    }

    return played.b_tie;
}



/**
 * @brief  Puts the pattern and the clock at their start. On the internal
 *         clock the first step is due at once; on the external one it is a
 *         pulse short of due, which the first edge brings.
 * @param  p_state  Sequencer state; the pitch CV value is kept.
 * @param  mode     Which clock the pattern is to run from.
 */
static void start(seq_state_t *p_state, transport_mode_t mode)
{
    clock_init(&p_state->clock);
    engines_init(p_state);
    gate_init(&p_state->gate);
    p_state->step_pulses = SEQ_PULSES_PER_STEP;
    p_state->leds        = 0U;
    if (TRANSPORT_EXTERNAL == mode)
    {
        p_state->step_pulses = SEQ_PULSES_PER_STEP - 1U;
    }
}



/**
 * @brief  Runs one tick of a pattern that is running: moves the clock on,
 *         begins a step if one is due, and gives the gate and clock levels.
 *         On the external clock, a step that falls due while the clock
 *         output is still high from the step before waits one tick, with
 *         the gate closed and the clock output low, so that it has a rising
 *         edge of its own.
 * @param  p_state        Sequencer state; updated.
 * @param  p_in           Inputs for this tick.
 * @param  mode           Which clock the pattern is running from.
 * @param  elapsed_ticks  Ticks to move the clock on by.
 * @param  p_out          Outputs, with the step fields already cleared; the
 *                        pulses that fell are given, whether or not a step
 *                        begins.
 */
static void run(seq_state_t *p_state, const seq_inputs_t *p_in, transport_mode_t mode,
                uint8_t elapsed_ticks, seq_outputs_t *p_out)
{
    // The level the clock output was left at by the tick before
    const bool b_clock_was_high = (p_state->step_pulses < SEQ_CLOCK_OUT_PULSES);
    uint8_t    pulses;
    uint16_t   gate_pulses;

    if (p_in->b_reset)
    {
        engines_init(p_state);
    }

    pulses               = pulses_due(p_state, p_in, mode, elapsed_ticks);
    p_state->step_pulses = (uint16_t)(p_state->step_pulses + pulses);
    gate_pulses          = pulses;
    p_out->pulses        = pulses;

    if ((TRANSPORT_EXTERNAL == mode) && b_clock_was_high
        && (p_state->step_pulses >= SEQ_PULSES_PER_STEP))
    {
        // An edge that came early. The step stays due and begins on the
        // next tick; this one is the gap between the two
        gate_init(&p_state->gate);
    }
    else if (p_state->step_pulses >= SEQ_PULSES_PER_STEP)
    {
        gate_config_t gate;

        p_state->step_pulses = (uint16_t)(p_state->step_pulses - SEQ_PULSES_PER_STEP);
        gate.length          = p_in->gate.length;
        if (begin_step(p_state, p_in, p_out) && (gate.length < SEQ_PULSES_PER_STEP))
        {
            // A tied note lasts until the next step begins, at least
            gate.length = (uint8_t)SEQ_PULSES_PER_STEP;
        }

        // The boundary ends the last step's gate, and only a pitch opens
        // another. Its length counts from the boundary, so only the
        // pulses already into the new step come off it
        gate_begin(&p_state->gate, &gate, p_out->b_note_valid && !p_out->b_rest);
        gate_pulses = p_state->step_pulses;
    }
    else
    {
        // No step is due
    }

    p_out->b_gate      = gate_advance(&p_state->gate, gate_pulses);
    p_out->b_clock_out = (p_state->step_pulses < SEQ_CLOCK_OUT_PULSES);
    if (p_out->b_clock_out)
    {
        p_out->leds = p_state->leds;
    }
}



/**
 * @brief  Runs one tick of a pattern that is stopped: closes the gate, and
 *         with the sync switch off keeps the internal clock going at the
 *         tempo, for its pulses alone. Nothing of the pattern moves, and a
 *         start puts the clock back to its beginning. With the switch on
 *         the clock is left alone and no pulse falls.
 * @param  p_state        Sequencer state; the gate and the clock change.
 * @param  b_external     The sync switch's settled level.
 * @param  elapsed_ticks  Ticks to move the clock on by.
 * @return Pulses that fell.
 */
static uint8_t idle(seq_state_t *p_state, bool b_external,
                    uint8_t elapsed_ticks)
{
    uint8_t pulses = 0U;

    gate_init(&p_state->gate);
    if (!b_external)
    {
        pulses = clock_advance(&p_state->clock, tempo_bpm(p_state->last_tempo_raw),
                               elapsed_ticks);
    }

    return pulses;
}



void seq_init(seq_state_t *p_state)
{
    if (NULL != p_state)
    {
        p_state->last_tempo_raw = 0U;
        p_state->cv             = 0U;
        p_state->engine         = (uint8_t)ENGINE_PLAIN;
        prng_init(&p_state->prng, (uint16_t)PRNG_SEED);
        transport_init(&p_state->transport);
        start(p_state, TRANSPORT_INTERNAL);
    }
}



void seq_tick(seq_state_t *p_state, const seq_inputs_t *p_in, seq_outputs_t *p_out)
{
    if ((NULL != p_state) && (NULL != p_in) && (NULL != p_out))
    {
        transport_inputs_t  controls;
        transport_outputs_t transport;
        uint8_t             elapsed_ticks = p_in->elapsed_ticks;

        controls.b_run_button  = p_in->b_run_button;
        controls.b_sync_switch = p_in->b_sync_switch;
        controls.elapsed_ticks = p_in->elapsed_ticks;
        transport_tick(&p_state->transport, &controls, &transport);

        // An invalid tempo reading leaves the last valid one in place
        if (p_in->b_tempo_valid)
        {
            p_state->last_tempo_raw = p_in->tempo_raw;
        }

        if (transport.b_started)
        {
            // The time that went by before the start is not the pattern's
            start(p_state, transport.mode);
            elapsed_ticks = 0U;
        }

        p_out->b_step_started = false;
        p_out->step           = 0U;
        p_out->b_note_valid   = false;
        p_out->b_rest         = false;
        p_out->semitone       = 0U;
        p_out->b_gate         = false;
        p_out->b_clock_out    = false;
        p_out->leds           = 0U;

        if (TRANSPORT_STOPPED == transport.mode)
        {
            p_out->pulses = idle(p_state, transport.b_external, elapsed_ticks);
        }
        else
        {
            run(p_state, p_in, transport.mode, elapsed_ticks, p_out);
        }

        p_out->cv                  = p_state->cv;
        p_out->transport           = transport.mode;
        p_out->b_transport_changed = transport.b_changed;
    }
}
