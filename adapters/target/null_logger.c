/**
 * @file   null_logger.c
 * @brief  Target implementation of the log port for the release build: every
 *         message is discarded and the USART is left untouched. The debug
 *         build links serial_logger.c instead.
 * @author BonelessPig
 *
 * @copyright Copyright (c) 2026
 *
 */
#include "logger.h"
#include <stdint.h>
#include "log_port.h"
#include "port_status.h"



/**
 * @brief Does nothing; there is no transport to set up (see logger.h).
 * @param level ignored
 * @return port_status_t always STATUS_OK
 */
port_status_t logger_init(log_level_t level)
{
    (void)level;
    return STATUS_OK;
}



/**
 * @brief Discards a step note message (see log_port.h).
 * @param step ignored
 * @param note ignored
 */
void log_step_note(uint8_t step, uint8_t note)
{
    (void)step;
    (void)note;
}



/**
 * @brief Discards an error message (see log_port.h).
 * @param what ignored
 * @param status ignored
 */
void log_error(log_error_id_t what, port_status_t status)
{
    (void)what;
    (void)status;
}



/**
 * @brief Does nothing; no message is ever queued (see log_port.h).
 */
void log_poll(void)
{
}



/**
 * @brief Does nothing; there is never anything queued (see log_port.h).
 */
void log_flush(void)
{
}
