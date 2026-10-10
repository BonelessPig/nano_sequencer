#ifndef CLOCK_H
#define CLOCK_H
/**
 * @file   clock.h
 * @brief  Musical clock: turns elapsed timebase ticks into pulses at a tempo.
 *         Pure logic with no hardware access; compiles unchanged for the MCU
 *         and for a PC.
 * @author BonelessPig
 *
 * A tick is one millisecond. A pulse is 1/96 of a quarter note. At a tempo of
 * B beats per minute there are B * 96 pulses in 60000 ticks, which is
 * B * 8 pulses in 5000 ticks. The clock therefore adds B * 8 to a phase
 * counter on every tick and emits one pulse each time the counter passes
 * 5000, keeping the remainder. Nothing is rounded, so the pulse count never
 * drifts from the tempo.
 *
 * @copyright Copyright (c) 2026
 *
 */
#include <stdint.h>

#define CLOCK_PPQN    (96U)  // Pulses per quarter note
#define CLOCK_BPM_MAX (300U) // Fastest tempo the clock runs at, in beats per minute

/**
 * @brief State the clock carries from one call to the next.
 */
typedef struct
{
    uint16_t phase; // Fraction of a pulse built up so far, 0 to 4999
} clock_state_t;

/**
 * @brief  Puts the clock at the start of a pulse.
 * @param  p_state  State to initialize. Ignored if null.
 */
void clock_init(clock_state_t *p_state);

/**
 * @brief  Moves the clock on by a number of ticks and reports the pulses
 *         that fell due in that time. Reads and writes *p_state and has no
 *         other effect.
 * @param  p_state        State from clock_init() or the previous call; updated.
 * @param  bpm            Tempo in beats per minute, 0 to CLOCK_BPM_MAX. A
 *                        larger value is treated as CLOCK_BPM_MAX. 0 stops
 *                        the clock: no pulses, and the phase is kept.
 * @param  elapsed_ticks  Ticks (milliseconds) since the previous call, 0 to 255.
 * @return Pulses that fell due, 0 to elapsed_ticks (at most one per tick).
 *         0 if p_state is null.
 */
uint8_t clock_advance(clock_state_t *p_state, uint16_t bpm, uint8_t elapsed_ticks);

#endif /* CLOCK_H */
