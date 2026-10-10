#ifndef SPI_H
#define SPI_H
/**
 * @file   spi.h
 * @brief  SPI driver for the target: the MCU as bus master. Private to
 *         adapters/target/; the device adapters that sit on the bus (the
 *         shift register reader and the pitch CV output) are its only
 *         callers, and it implements no port of its own.
 * @author BonelessPig
 *
 * The bus runs in SPI mode 0 (clock idle low, data sampled on the rising
 * edge), most significant bit first, at 1 MHz (system clock / 16). The pins
 * are PB5 (SCK), PB4 (MISO) and PB3 (MOSI). PB2 (SS) is made an output and
 * left high: as an input, a low level on it would drop the MCU out of master
 * mode. As an output it is free to be a chip select, and the pitch CV output
 * uses it as the DAC's.
 *
 * @copyright Copyright (c) 2026
 *
 */
#include <stdint.h>
#include "port_status.h"

/**
 * @brief  Sets up the bus pins and switches the SPI on as master. Call once
 *         in the platform bring-up, before anything uses the bus.
 * @note   Not ISR-safe. Call from the main context only.
 */
void spi_init(void);

/**
 * @brief  Sends one byte and receives one byte, which on SPI is the same
 *         eight clock pulses.
 * @param  tx    Byte to send, most significant bit first.
 * @param  p_rx  Receives the byte read in during the same eight clocks, most
 *               significant bit first. Must not be null.
 * @return STATUS_OK on success; ERR_INVALID_PARAM for a null p_rx, in which
 *         case nothing is sent; ERR_TIMEOUT if the transfer did not complete,
 *         in which case *p_rx is not written.
 * @note   Not ISR-safe. Blocks for the transfer: 8 microseconds at 1 MHz.
 *         The wait is bounded, at well over ten times that.
 */
port_status_t spi_transfer(uint8_t tx, uint8_t *p_rx);

#endif /* SPI_H */
