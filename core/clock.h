#ifndef CLOCK_H
#define CLOCK_H
/**
 * @file   clock.h
 * @brief  Musical clock: turns elapsed timebase ticks into pulses, at a
 *         tempo or in step with the edges of an external clock. Pure logic
 *         with no hardware access; compiles unchanged for the MCU and for a
 *         PC.
 * @author BonelessPig
 *
 * A tick is one millisecond. A pulse is 1/96 of a quarter note.
 *
 * At a tempo (clock_advance()): at B beats per minute there are B * 96
 * pulses in 60000 ticks, which is B * 8 pulses in 5000 ticks. The clock
 * therefore adds B * 8 to a phase counter on every tick and emits one pulse
 * each time the counter passes 5000, keeping the remainder. Nothing is
 * rounded, so the pulse count never drifts from the tempo.
 *
 * Following external edges (clock_follow()): every divide-th edge is a sync
 * edge, and from one sync edge to the next the clock gives exactly
 * 24 * multiply pulses, so that at a ratio of one an edge is a sixteenth
 * note. It spaces them by the time the last two sync edges were apart, stops
 * one short if the next sync edge is late, and gives whatever is left when
 * it comes. The caller's steps therefore begin on sync edges, with
 * multiply - 1 more fitted in between.
 *
 * The first sync edge after clock_init() or clock_advance() has no spacing
 * to go by. Until the second, the clock takes the edges to be 20 ticks apart
 * and the ratio's multiply to be one. The same happens after a gap of 4000
 * ticks or more.
 *
 * @copyright Copyright (c) 2026
 *
 */
#include <stdbool.h>
#include <stdint.h>

#define CLOCK_PPQN            (96U)  // Pulses per quarter note
#define CLOCK_BPM_MAX         (300U) // Fastest tempo the clock runs at, in beats per minute
#define CLOCK_PULSES_PER_EDGE (24U)  // Pulses to an external edge at a ratio of one
#define CLOCK_MULTIPLY_MAX    (4U)   // Most a ratio can multiply the external clock by

/**
 * @brief State the clock carries from one call to the next.
 */
typedef struct
{
    uint16_t phase;      // Fraction of a pulse built up so far
    bool     b_synced;   // true once a sync edge has been followed
    uint16_t period;     // Ticks from one sync edge to the next, as last measured
    uint16_t sync_ticks; // Ticks since the last sync edge, up to 4000
    uint8_t  per_sync;   // Pulses to give between this sync edge and the next
    uint8_t  given;      // Pulses given since the last sync edge, below per_sync
    uint8_t  edges_left; // Edges to pass over before the next sync edge
} clock_state_t;

/**
 * @brief What clock_follow() is given on each call.
 */
typedef struct
{
    bool    b_edge;         // true if an external edge arrived since the last call
    uint8_t elapsed_ticks;  // Ticks (milliseconds) since the previous call, 0 to 255
    uint8_t divide;         // Edges to a sync edge, 1 to 255. 0 is taken as 1
    uint8_t multiply;       // 1 to CLOCK_MULTIPLY_MAX: steps to a sync edge. 0 is
                            // taken as 1 and a larger value as the maximum
    uint8_t pulses_to_step; // Pulses still to go before the caller's next step
} clock_follow_t;

/**
 * @brief  Puts the clock at the start of a pulse, following nothing.
 * @param  p_state  State to initialize. Ignored if null.
 */
void clock_init(clock_state_t *p_state);

/**
 * @brief  Moves the clock on by a number of ticks at a tempo and reports the
 *         pulses that fell due in that time. Reads and writes *p_state and
 *         has no other effect. It also ends any following of external edges:
 *         the next clock_follow() starts afresh.
 * @param  p_state        State from clock_init() or the previous call; updated.
 * @param  bpm            Tempo in beats per minute, 0 to CLOCK_BPM_MAX. A
 *                        larger value is treated as CLOCK_BPM_MAX. 0 stops
 *                        the clock: no pulses, and the phase is kept.
 * @param  elapsed_ticks  Ticks (milliseconds) since the previous call, 0 to 255.
 * @return Pulses that fell due, 0 to elapsed_ticks (at most one per tick).
 *         0 if p_state is null.
 */
uint8_t clock_advance(clock_state_t *p_state, uint16_t bpm, uint8_t elapsed_ticks);

/**
 * @brief  Moves the clock on by a number of ticks in step with an external
 *         clock, as the top of this file describes, and reports the pulses
 *         that fell due. Reads *p_state and *p_in, writes *p_state, and has
 *         no other effect.
 *
 *         The first sync edge after clock_init() or clock_advance() returns
 *         p_in->pulses_to_step, so that the caller's next step begins on it
 *         wherever the caller had got to. Before it, pulses come 24 to every
 *         20 ticks and stop one short of that step, so that a step begun at
 *         a tempo is finished and the next waits for the edge.
 *
 *         The ratio is read at each sync edge and holds until the next.
 * @param  p_state  State from clock_init() or the previous call; updated.
 * @param  p_in     The edge, the elapsed time and the ratio for this call.
 * @return Pulses that fell due, 0 to 96, or p_in->pulses_to_step on the
 *         first sync edge. 0 if either pointer is null.
 */
uint8_t clock_follow(clock_state_t *p_state, const clock_follow_t *p_in);

#endif /* CLOCK_H */
