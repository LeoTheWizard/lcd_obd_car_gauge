///
/// @file spi.h
/// @brief Manages the SPI bus hardware on the rp2350 microcontroller.
/// @author Leo Walker
///

#pragma once

#include <stdint.h>
#include <stdbool.h>

///
/// @brief Represents a hardware block on the pico that spi can used on.
/// @note pass into the spi device config.
///
typedef enum
{
    SPI_HW_0,
    SPI_HW_1,
} spi_interface;

///
/// @brief Represents a spi device context.
///
typedef struct spi_instance spi_instance_t;

///
/// @brief Define and initialize a spi instance.
/// @param hardware The hardware block to drive spi on gpio pins.
/// @param sclk_pin The spi master clock pin number.
/// @param mosi_pin The spi master out, slave in pin number.
/// @param miso_pin The spi master in, slave out pin number.
/// @param cs_pin The chip select pin number.
/// @param invert_cs If the chip select needs to be inverted.
/// @param bitrate The bitrate of the SPI bus.
/// @return A pointer to a spi device context, pass this into relevant functions.
///
spi_instance_t* spi_instance_init(spi_interface hardware, const uint8_t sclk_pin, const uint8_t mosi_pin, const uint8_t miso_pin, const uint8_t cs_pin, const bool invert_cs, const uint32_t bitrate);

///
/// @brief Clean up the spi context and hardware by passing the pointer into this function.
/// @param dev The spi device context.
///
void spi_instance_destroy(spi_instance_t* dev);

///
/// @brief Enable the chip select pin.
/// @param dev The spi device context.
///
void spi_instance_chip_enable(const spi_instance_t* dev);

///
/// @brief Disable the chip select pin.
/// @param dev The spi device context.
///
void spi_instance_chip_disable(const spi_instance_t* dev);

///
/// @brief Transfers data from the transmit_buffer and recieves the return data into the recieve_buffer.
/// @param dev The spi device context.
/// @param transmit_buffer The data buffer to transmit.
/// @param recieve_buffer The data buffer to recieve data into.
/// @param length The number of bytes to transfer.
/// @warning Ensure length does not exceed either buffer sizes.
///
void spi_instance_transfer(const spi_instance_t* dev, const uint8_t* transmit_buffer, uint8_t* recieve_buffer, uint32_t length);

///
/// @brief Transmit data from a buffer.
/// @param dev The spi device context.
/// @param transmit_buffer The data buffer to write from.
/// @param length The number of bytes to write.
/// @warning Ensure length does not exceed buffer size.
///
void spi_instance_transmit(const spi_instance_t* dev, const uint8_t* transmit_buffer, uint32_t length);

///
/// @brief Transmit a single byte.
/// @param dev The spi device context.
/// @param data The byte of data to send.
///
void spi_instance_transmit_byte(const spi_instance_t* dev, const uint8_t data);

///
/// @brief Read data into a buffer.
/// @param dev The spi device context.
/// @param recieve_buffer The data buffer to fill.
/// @param length The number of bytes to read.
/// @warning Ensure length does not exceed buffer size.
///
void spi_instance_recieve(const spi_instance_t* dev, uint8_t* recieve_buffer, uint32_t length);

///
/// @brief Read a single byte.
/// @details Will transmit a single 0x00 to clock out the slave.
/// @param dev The spi device context.
/// @return The recieved byte.
///
uint8_t spi_instance_recieve_byte(const spi_instance_t* dev);

