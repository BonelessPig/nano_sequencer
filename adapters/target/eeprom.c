/**
 * @file   eeprom.c
 * @brief  Target implementation of the non-volatile storage port: the MCU's
 *         own 1024-byte EEPROM.
 * @author BonelessPig
 *
 * A byte takes about 3.4 ms to write, so nothing here waits for one. A write
 * is copied into a small buffer, and nv_poll() starts one byte each time the
 * EEPROM is free. A byte that already holds the value wanted is not written
 * again, which saves the time and the wear.
 *
 * Starting a byte is a timed sequence: the write enable bit only takes if it
 * is set within four clock cycles of the master write enable bit, so the
 * tick interrupt is held off for those two instructions.
 *
 * @copyright Copyright (c) 2026
 *
 */
#include "nv_port.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "atmega328p_eeprom_regs.h"
#include "atmega328p_regs.h"
#include "port_status.h"

#define EEPROM_SIZE_BYTES (1024U)
#define BITS_PER_BYTE     (8U)
#define LOW_BYTE_MASK     (0xFFU)

static uint8_t  g_write_data[NV_WRITE_BYTES_MAX]; // The bytes of the write in hand
static uint16_t g_write_address = 0U;             // Where the next of them goes
static uint8_t  g_write_next    = 0U;             // Index of the next to write
static uint8_t  g_write_count   = 0U;             // How many there are in all



/**
 * @brief  Tells whether a run of bytes lies inside the EEPROM.
 * @param  address  Address of the first byte.
 * @param  count    Number of bytes.
 * @return true if there is at least one byte and all of them fit.
 */
static bool is_run_valid(uint16_t address, uint8_t count)
{
    return (count > 0U) && (address < EEPROM_SIZE_BYTES) &&
           (count <= (uint16_t)(EEPROM_SIZE_BYTES - address));
}



/**
 * @brief  Tells whether the EEPROM is part way through writing a byte.
 * @return true while the hardware holds its write enable bit set.
 */
static bool is_byte_in_progress(void)
{
    return (0U != (EECR & (1U << EEPE)));
}



/**
 * @brief  Reads one byte. The EEPROM must not be writing. The address is
 *         left in the address register.
 * @param  address  Address to read, 0 to EEPROM_SIZE_BYTES - 1.
 * @return The byte stored there.
 */
static uint8_t read_byte(uint16_t address)
{
    EEARH = (uint8_t)(address >> BITS_PER_BYTE);
    EEARL = (uint8_t)(address & LOW_BYTE_MASK);
    EECR |= (uint8_t)(1U << EERE); // The data register holds the byte at once

    return EEDR;
}



/**
 * @brief  Starts the write of one byte to the address already in the address
 *         register. The EEPROM must not be writing.
 * @param  value  The byte to store.
 */
static void start_byte(uint8_t value)
{
    const uint8_t saved_sreg = SREG;

    EEDR = value;

    // Erase and write in one operation, with no interrupt between arming the
    // write and starting it
    SREG  = (uint8_t)(saved_sreg & (uint8_t)~(1U << SREG_I));
    EECR  = (uint8_t)(1U << EEMPE);
    EECR |= (uint8_t)(1U << EEPE);
    SREG  = saved_sreg;
}



/**
 * @brief  Reads bytes from the EEPROM (see nv_port.h).
 * @param  address  address of the first byte
 * @param  p_data   receives the bytes
 * @param  count    number of bytes
 * @return port_status_t status code (STATUS_OK for success)
 */
port_status_t nv_read(uint16_t address, uint8_t *p_data, uint8_t count)
{
    port_status_t status = STATUS_OK;

    if ((NULL == p_data) || !is_run_valid(address, count))
    {
        status = ERR_INVALID_PARAM;
    }
    else if ((g_write_next < g_write_count) || is_byte_in_progress())
    {
        status = ERR_BUSY; // A read would disturb the address of the write
    }
    else
    {
        for (uint8_t i = 0U; i < count; i++)
        {
            p_data[i] = read_byte((uint16_t)(address + i));
        }
    }

    return status;
}



/**
 * @brief  Takes a copy of the bytes for nv_poll() to write (see nv_port.h).
 * @param  address  address of the first byte
 * @param  p_data   the bytes to write
 * @param  count    number of bytes
 * @return port_status_t status code (STATUS_OK for success)
 */
port_status_t nv_write(uint16_t address, const uint8_t *p_data, uint8_t count)
{
    port_status_t status = STATUS_OK;

    if ((NULL == p_data) || (count > NV_WRITE_BYTES_MAX) || !is_run_valid(address, count))
    {
        status = ERR_INVALID_PARAM;
    }
    else if (g_write_next < g_write_count)
    {
        status = ERR_BUSY;
    }
    else
    {
        for (uint8_t i = 0U; i < count; i++)
        {
            g_write_data[i] = p_data[i];
        }
        g_write_address = address;
        g_write_next    = 0U;
        g_write_count   = count;
    }

    return status;
}



/**
 * @brief  Deals with the next byte of the write in hand, if there is one and
 *         the EEPROM is free: starts writing it, or passes over it if it is
 *         already there (see nv_port.h). Never waits.
 */
void nv_poll(void)
{
    if ((g_write_next < g_write_count) && !is_byte_in_progress())
    {
        const uint8_t value = g_write_data[g_write_next];

        if (value != read_byte(g_write_address))
        {
            start_byte(value);
        }
        g_write_next++;
        g_write_address++;
    }
}
