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
 *         platform fails to come up, the failure is logged.
 * @return STATUS_OK on success, otherwise the status platform_init() returned.
 */
port_status_t app_init(void);

/**
 * @brief  Runs one tick of the sequencer: gathers the inputs from the ports,
 *         runs the core, then applies its outputs (logging and the delay).
 *         Blocks for the logging time plus the tempo delay.
 */
void app_run_once(void);

#endif /* APP_H */
