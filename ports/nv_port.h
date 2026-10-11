#ifndef NV_PORT_H
#define NV_PORT_H
/**
 * @file   nv_port.h
 * @brief  Port: storage that keeps its contents with the power off, as bytes
 *         at addresses counted from 0. Writing is slow (milliseconds a byte
 *         on the target), so a write is only started here and is carried out
 *         a byte at a time by nv_poll().
 * @author BonelessPig
 *
 * @copyright Copyright (c) 2026
 *
 */
#include <stdint.h>
#include "port_status.h"

#define NV_WRITE_BYTES_MAX (16U) // The most one nv_write() call can take

/**
 * @brief  Reads bytes from the storage.
 * @param  address  Address of the first byte. The whole run must lie inside
 *                  the storage (1024 bytes on the target).
 * @param  p_data   Receives the bytes. Left untouched unless the result is
 *                  STATUS_OK.
 * @param  count    Number of bytes, 1 or more.
 * @return STATUS_OK on success; ERR_INVALID_PARAM for a null pointer, a
 *         count of 0 or a run that leaves the storage; ERR_BUSY while a
 *         write is still being carried out.
 * @note   Not ISR-safe. Does not wait: a few microseconds a byte on the
 *         target.
 */
port_status_t nv_read(uint16_t address, uint8_t *p_data, uint8_t count);

/**
 * @brief  Starts writing bytes to the storage. The bytes are copied, so the
 *         caller's buffer is free as soon as this returns; nv_poll() then
 *         writes them one at a time, lowest address first. If the power
 *         goes before it has finished, only the first part has been written.
 * @param  address  Address of the first byte. The whole run must lie inside
 *                  the storage.
 * @param  p_data   The bytes to write.
 * @param  count    Number of bytes, 1 to NV_WRITE_BYTES_MAX.
 * @return STATUS_OK if the write was taken; ERR_INVALID_PARAM for a null
 *         pointer, a count out of range or a run that leaves the storage;
 *         ERR_BUSY if an earlier write has bytes still to go, in which case
 *         this one is not taken.
 * @note   Not ISR-safe. Does not block.
 */
port_status_t nv_write(uint16_t address, const uint8_t *p_data, uint8_t count);

/**
 * @brief  Carries a write forward by at most one byte, if the storage is
 *         ready for it. A byte that already holds the value wanted is passed
 *         over without being written. Call on every pass of the main loop.
 * @note   Not ISR-safe. Never blocks. On the target a byte takes about
 *         3.4 ms to write, and interrupts are held off for a few clock
 *         cycles as each one is started.
 */
void nv_poll(void);

#endif /* NV_PORT_H */
