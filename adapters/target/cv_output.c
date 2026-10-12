/**
 * @file   cv_output.c
 * @brief  Target implementation of the pitch CV and the envelope CV output
 *         ports: channels A and B of an MCP4822 12-bit DAC on the SPI bus.
 * @author BonelessPig
 *
 * The DAC takes one 16-bit frame for one channel, most significant bit
 * first, while its chip select is low: four configuration bits, the first
 * of which says which channel, then the 12-bit value. With its
 * LDAC pin tied low the output changes as chip select goes back high, and
 * a frame cut short of 16 clocks is ignored. It runs from its internal 2.048 V
 * reference at a gain of 2, which makes one step of the value 1 mV and the
 * full range 0 to 4.095 V, so the value sent is the voltage in millivolts.
 * Both channels are run that way.
 *
 * Chip select is PB2, the pin the SPI driver sets up as a high output to
 * keep the MCU in master mode; this file only changes its level. The bus
 * clock and data out (PB5 and PB3) belong to the driver.
 *
 * @copyright Copyright (c) 2026
 *
 */
#include "cv_output_port.h"
#include "env_output_port.h"
#include <stdint.h>
#include "atmega328p_regs.h"
#include "bits.h"
#include "port_status.h"
#include "spi.h"

#define CV_SELECT_BIT BIT_2 // PB2: the DAC's chip select, active low

// The configuration bits, as they sit in the first byte of the frame.
// Bit 7: clear for channel A, set for channel B. Bit 5 clear: gain of 2.
// Bit 4 set: output on.
#define DAC_CONFIG_A    (0x10U)
#define DAC_CONFIG_B    (0x90U)
#define BITS_PER_BYTE   (8U)
#define LOW_BYTE_MASK   (0xFFU)



/**
 * @brief Sends a voltage to one channel of the DAC as one frame. Chip select
 *        is raised again whether or not both bytes went out: a frame cut
 *        short is one the DAC ignores, so a failed write leaves the output
 *        as it was.
 * @param config the configuration bits, which name the channel
 * @param millivolts voltage to set, 0 to CV_OUTPUT_MILLIVOLTS_MAX
 * @return port_status_t status code (STATUS_OK for success)
 */
static port_status_t dac_write(uint8_t config, uint16_t millivolts)
{
    port_status_t status = STATUS_OK;

    if (millivolts > CV_OUTPUT_MILLIVOLTS_MAX)
    {
        status = ERR_INVALID_PARAM;
    }
    else
    {
        // At most 12 bits, so its top four fill the rest of the first byte
        const uint8_t high_bits = (uint8_t)(millivolts >> BITS_PER_BYTE);
        const uint8_t first     = (uint8_t)(config | high_bits);
        const uint8_t second    = (uint8_t)(millivolts & LOW_BYTE_MASK);
        uint8_t       unused    = 0U; // The DAC sends nothing back

        PORTB &= (uint8_t)~CV_SELECT_BIT; // Select: the frame begins

        status = spi_transfer(first, &unused);
        if (STATUS_OK == status)
        {
            status = spi_transfer(second, &unused);
        }

        PORTB |= CV_SELECT_BIT; // Deselect: a whole frame takes effect here
    }

    return status;
}



/**
 * @brief Sets the pitch CV, on channel A (see cv_output_port.h).
 * @param millivolts voltage to set, 0 to CV_OUTPUT_MILLIVOLTS_MAX
 * @return port_status_t status code (STATUS_OK for success)
 */
port_status_t cv_output_write(uint16_t millivolts)
{
    return dac_write((uint8_t)DAC_CONFIG_A, millivolts);
}



/**
 * @brief Sets the envelope CV, on channel B (see env_output_port.h). Its
 *        range is the pitch CV's.
 * @param millivolts voltage to set, 0 to CV_OUTPUT_MILLIVOLTS_MAX
 * @return port_status_t status code (STATUS_OK for success)
 */
port_status_t env_output_write(uint16_t millivolts)
{
    return dac_write((uint8_t)DAC_CONFIG_B, millivolts);
}
