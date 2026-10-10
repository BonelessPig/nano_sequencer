/**
 * @file   spi.c
 * @brief  SPI driver for the target: the MCU as bus master (see spi.h).
 * @author BonelessPig
 *
 * @copyright Copyright (c) 2026
 *
 */
#include "spi.h"
#include <stdbool.h>
#include <stddef.h>
#include "atmega328p_regs.h"
#include "atmega328p_spi_regs.h"
#include "bits.h"
#include "port_status.h"

#define SPI_SS_BIT   BIT_2 // PB2: SS, an output so the MCU stays master; idles high
#define SPI_MOSI_BIT BIT_3 // PB3: MOSI, data out to the parts that listen
#define SPI_SCK_BIT  BIT_5 // PB5: SCK, the bus clock

// Upper bound on polls of the transfer-complete flag. A transfer takes 128
// CPU clocks (8 bits at system clock / 16), which is about 15 polls; this
// many only runs out if the SPI is not switched on as master.
#define SPI_TIMEOUT_LOOPS (255U)



void spi_init(void)
{
    // SS goes high before it becomes an output, so it is never driven low,
    // and is an output before master mode is selected, so a low level on the
    // pin cannot cancel it
    PORTB |= SPI_SS_BIT;
    DDRB  |= SPI_SS_BIT;
    DDRB  |= SPI_MOSI_BIT;
    DDRB  |= SPI_SCK_BIT;

    // Set these explicitly rather than relying on reset defaults, in case a
    // bootloader left them changed. Mode 0, most significant bit first, no
    // interrupt, system clock / 16; and no double speed
    SPCR = (uint8_t)((1U << SPE) | (1U << MSTR) | (1U << SPR0));
    SPSR = 0U;
}



port_status_t spi_transfer(uint8_t tx, uint8_t *p_rx)
{
    port_status_t status = STATUS_OK;

    if (NULL == p_rx)
    {
        status = ERR_INVALID_PARAM;
    }
    else
    {
        uint8_t loops_left = SPI_TIMEOUT_LOOPS;
        bool    b_busy     = true;

        SPDR = tx; // Starts the eight clocks

        // Wait for the transfer to complete, giving up once the poll budget is spent
        while (b_busy && (loops_left > 0U))
        {
            if (0U != (SPSR & (1U << SPIF)))
            {
                b_busy = false;
            }
            else
            {
                loops_left--;
            }
        }

        if (b_busy)
        {
            status = ERR_TIMEOUT; // The SPI never finished (e.g. not enabled)
        }
        else
        {
            *p_rx = SPDR; // Reading the data after the flag also clears the flag
        }
    }

    return status;
}
