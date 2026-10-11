/**
 * @file   shift_reg_reader.c
 * @brief  Target implementation of the step input port and the LED output
 *         port, which share the SPI bus and one control line: reads the
 *         74HC165 shift register chain that holds the step notes, the
 *         mode switches and the selector, and on the way writes the two 74HC595s that drive
 *         the sixteen step lights.
 * @author BonelessPig
 *
 * The chain's clock is the bus clock (PB5, SCK) and its output, QH of the
 * chip nearest the MCU, is the bus data input (PB4, MISO). Its load line is
 * an ordinary output, PB1. A 74HC165 shows its first bit as soon as it is
 * loaded and moves to the next on each rising clock edge, which is the edge
 * SPI mode 0 samples on, so each transfer reads eight bits in order.
 *
 * The two 74HC595s take their data from the bus data output (PB3, MOSI),
 * the far one from the near one's serial output, and shift on the same
 * clock. They have no chip select, so every byte sent on the bus goes
 * through them; what counts is what they hold when their latch input rises,
 * and that input is PB1 as well. So a read first sends the two bytes the
 * lights are to show, and the rising edge that ends the 74HC165 load pulse
 * puts them on the 74HC595 outputs. Everything sent afterwards (the zeros of
 * the read, the DAC's frames) passes through unseen until the next read
 * sends the lights again.
 *
 * @copyright Copyright (c) 2026
 *
 */
#include "step_input_port.h"
#include <stddef.h>
#include "led_output_port.h"
#include "atmega328p_regs.h"
#include "bits.h"
#include "port_status.h"
#include "spi.h"

// Daisy-chained 74HC165s: eight for the steps, one for the mode switches
// and, farthest from the MCU, one for the selector (10 x 8 bits = 80 bits)
#define SHIFT_REG_CHAIN_BYTES (10U)

// PB1: SH/LD of the 74HC165s (active-low load pulse) and the latch clock
// of the 74HC595s (rising edge)
#define SHIFT_LOAD_BIT BIT_1

#define SHIFT_FILL_BYTE (0U) // Sent while reading; nothing on the bus keeps it

#define LED_HIGH_BYTE_SHIFT (8U)
#define LED_LOW_BYTE_MASK   (0x00FFU)

// What the step lights are to show; sent ahead of the next load pulse
static uint16_t g_step_leds = 0U;



void led_output_write(uint16_t leds)
{
    g_step_leds = leds;
}



/**
 * @brief  Sends the 74HC595s what the step lights are to show: steps 15 to
 *         8, then 7 to 0, the highest first, so that step 0 ends up on
 *         output QA of the chip nearest the MCU and step 15 on QH of the
 *         far one. Nothing shows until the latch input next rises.
 * @return STATUS_OK, or the SPI driver's status if a transfer failed.
 */
static port_status_t send_leds(void)
{
    uint8_t       unused = 0U;
    port_status_t status =
        spi_transfer((uint8_t)(g_step_leds >> LED_HIGH_BYTE_SHIFT), &unused);

    if (STATUS_OK == status)
    {
        status = spi_transfer((uint8_t)(g_step_leds & LED_LOW_BYTE_MASK), &unused);
    }

    return status;
}



/**
 * @brief Performs one parallel-load + serial-clock-out cycle across the 74HC165
 *        daisy chain and stores the raw bits read (see step_input_port.h).
 * @param p_raw_bits pointer to a buffer of at least byte_count bytes. p_raw_bits[0]
 *                   holds the first 8 bits clocked out (the chip nearest the MCU,
 *                   whose QH feeds the MCU's data-in pin); p_raw_bits[byte_count-1]
 *                   holds the chip farthest from the MCU (SER tied low).
 * @param byte_count number of bytes to read (i.e. number of chained 74HC165s);
 *                   must be between 1 and SHIFT_REG_CHAIN_BYTES
 * @return port_status_t status code (STATUS_OK for success)
 */
port_status_t step_input_read(uint8_t *p_raw_bits, uint8_t byte_count)
{
    port_status_t status = STATUS_OK;

    if ((NULL == p_raw_bits) || (0U == byte_count) || (byte_count > SHIFT_REG_CHAIN_BYTES))
    {
        status = ERR_INVALID_PARAM;
    }
    else
    {
        // Read into a buffer of our own, so that a read that fails part way
        // leaves the caller's untouched
        uint8_t bytes[SHIFT_REG_CHAIN_BYTES];
        uint8_t count = 0U;

        status = send_leds();
        if (STATUS_OK == status)
        {
            PORTB &= (uint8_t)~SHIFT_LOAD_BIT; // Latch parallel inputs
            PORTB |= SHIFT_LOAD_BIT;  // Return to shift mode; the lights change
        }

        while ((STATUS_OK == status) && (count < byte_count))
        {
            status = spi_transfer(SHIFT_FILL_BYTE, &bytes[count]);
            count++;
        }

        if (STATUS_OK == status)
        {
            for (uint8_t i = 0U; i < byte_count; i++)
            {
                p_raw_bits[i] = bytes[i];
            }
        }
    }

    return status;
}
