#ifndef TRANSPORT_H
#define TRANSPORT_H
/**
 * @file   transport.h
 * @brief  Transport: whether the pattern is running, and from which clock.
 *         Pure logic with no hardware access; compiles unchanged for the MCU
 *         and for a PC.
 * @author BonelessPig
 *
 * The run button turns running on and off, one change per press. It is
 * debounced here (see debounce.h), so it is given as read.
 *
 * The clock chooses itself. An edge on the external clock takes the pattern
 * over, and it stays on the external clock from then on, edges or none. A
 * stop forgets the external clock, so the start after it is on the internal
 * one; an edge that comes while the pattern is stopped brings the external
 * clock back, so a start with a clock running waits for its next edge. The
 * transport never changes clock by itself for want of edges.
 *
 * The pattern is running after transport_init(), on the internal clock. A
 * button that is already held down then is not a press.
 *
 * @copyright Copyright (c) 2026
 *
 */
#include <stdbool.h>
#include <stdint.h>
#include "debounce.h"

/**
 * @brief What the pattern is doing. The numbers appear in the serial log,
 *        so they are fixed.
 */
typedef enum
{
    TRANSPORT_STOPPED  = 0, // Nothing plays
    TRANSPORT_INTERNAL = 1, // Running from the internal clock, at the tempo
    TRANSPORT_EXTERNAL = 2  // Running from the edges of the external clock
} transport_mode_t;

/**
 * @brief State the transport carries from one tick to the next.
 */
typedef struct
{
    debounce_state_t run_button; // Settles the run button
    bool             b_pressed;  // The run button's settled level last tick
    bool             b_run;      // true while the pattern is running
    bool             b_starting; // true until the start has been reported
    bool             b_external; // true once an external edge has come, until
                                 // the next stop
    transport_mode_t mode;       // The mode reported last tick
} transport_state_t;

/**
 * @brief The controls, as read on this tick.
 */
typedef struct
{
    bool    b_run_button;  // true while the run button is held down
    bool    b_clock_edge;  // true if an edge arrived on the external clock
                           // since the previous tick
    uint8_t elapsed_ticks; // Ticks (milliseconds) since the previous tick
} transport_inputs_t;

/**
 * @brief What the transport has decided for this tick.
 */
typedef struct
{
    transport_mode_t mode; // What the pattern is doing
    bool b_started;        // true on the tick the pattern starts running: the
                           // first after transport_init(), and each time the
                           // run button starts it again
    bool b_changed;        // true on a tick whose mode is not the last one's.
                           // The first tick is compared with
                           // TRANSPORT_INTERNAL
    bool b_external;       // true if the external clock is the one in use, or
                           // the one a start would use: an edge has come
                           // since the last stop. Given on every tick,
                           // stopped or running
} transport_outputs_t;

/**
 * @brief  Puts the transport into its power-on condition: running, on the
 *         internal clock, with the start still to be reported.
 * @param  p_state  State to initialize. Ignored if null.
 */
void transport_init(transport_state_t *p_state);

/**
 * @brief  Runs one tick: settles the run button, notes an external edge and
 *         works out the mode. A press that stops the pattern forgets the
 *         external clock; an edge on the same tick brings it straight back.
 *         Which clock is in use is given as well, since the mode does not
 *         say while the pattern is stopped. Reads *p_state and *p_in, writes
 *         *p_state and *p_out, and has no other effect.
 * @param  p_state  State from transport_init() or the previous tick; updated.
 * @param  p_in     The controls for this tick.
 * @param  p_out    Receives the outputs; every field is written.
 * @note   Does nothing if any pointer is null.
 */
void transport_tick(transport_state_t *p_state, const transport_inputs_t *p_in,
                    transport_outputs_t *p_out);

#endif /* TRANSPORT_H */
