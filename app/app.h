#ifndef APP_H
#define APP_H
/**
 * @file   app.h
 * @brief  The application: wires the ports to the sequencer core. main() on
 *         the target and the host tests both drive it through these two
 *         functions.
 * @author BonelessPig
 *
 * @copyright Copyright (c) 2026
 *
 */
#include "port_status.h"

/**
 * @brief  Brings up the platform and resets the sequencer state. If the
 *         platform fails to come up, the failure is logged and the log is
 *         sent in full before this returns.
 * @return STATUS_OK on success, otherwise the status platform_init() returned.
 */
port_status_t app_init(void);

/**
 * @brief  Runs one pass of the main loop. Call it continuously; it never
 *         waits. If one or more timebase ticks (milliseconds) have passed
 *         since the last pass, it runs one tick of the sequencer: applies the
 *         outputs the previous tick computed, gathers the inputs from the
 *         ports, then runs the core. Every output is therefore one tick late,
 *         by the same amount each time: the gate and the clock output change
 *         at the start of the tick after the one that worked them out. On
 *         every pass it also gives the log the chance to send a byte.
 */
void app_run_once(void);

#endif /* APP_H */
