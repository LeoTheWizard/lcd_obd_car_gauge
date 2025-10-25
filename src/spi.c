#include <stdlib.h>
#include "hardware/spi.h"
#include "hardware/gpio.h"
#include "spi.h"

struct spi_instance
{
    spi_inst_t* hw_inst;
    
    uint8_t pin_sclk;
    uint8_t pin_mosi;
    uint8_t pin_miso;
    uint8_t pin_cs;

    bool invert_cs;
};

spi_instance_t* spi_instance_init(spi_interface hardware, const uint8_t sclk_pin, const uint8_t mosi_pin, const uint8_t miso_pin, const uint8_t cs_pin, const bool invert_cs, const uint32_t bitrate)
{
    spi_instance_t *dev = malloc(sizeof(spi_instance_t));
    if(!dev) return NULL;

    __builtin_memset(dev, 0, sizeof(*dev));

    switch(hardware)
    {
        case SPI_HW_0:
        dev->hw_inst = spi0;
        break;

        case SPI_HW_1:
        dev->hw_inst = spi1;
        break;

        default:
        dev->hw_inst = NULL;
        break;
    }

    dev->pin_sclk = sclk_pin;
    dev->pin_mosi = mosi_pin;
    dev->pin_miso = miso_pin;
    dev->pin_cs = cs_pin;
    dev->invert_cs = invert_cs;

    // Initialise rp2350 spi hardware.
    spi_init(dev->hw_inst, bitrate);

    // Configure gpio pins for spi.
    gpio_set_function(dev->pin_sclk, GPIO_FUNC_SPI);
    gpio_set_function(dev->pin_mosi, GPIO_FUNC_SPI);
    gpio_set_function(dev->pin_miso, GPIO_FUNC_SPI);
    gpio_init(dev->pin_cs);
    gpio_set_dir(dev->pin_cs, GPIO_OUT);
    gpio_put(dev->pin_cs, 1);

    return dev; // Return pointer to new instance.
}

void spi_instance_destroy(spi_instance_t* dev)
{
    if(!dev) return;

    // Deinitialise gpio & spi hardware.
    spi_deinit(dev->hw_inst);
    gpio_deinit(dev->pin_sclk);
    gpio_deinit(dev->pin_mosi);
    gpio_deinit(dev->pin_miso);
    gpio_deinit(dev->pin_cs);

    // Clean up memory.
    free(dev);
}

void spi_instance_chip_enable(const spi_instance_t* dev)
{
    gpio_put(dev->pin_cs, dev->invert_cs);
}

void spi_instance_chip_disable(const spi_instance_t* dev)
{
    gpio_put(dev->pin_cs, !dev->invert_cs);
}

void spi_instance_transmit(const spi_instance_t* dev, const uint8_t* transmit_buffer, uint32_t length)
{
    spi_write_blocking(dev->hw_inst, transmit_buffer, length);
}

void spi_instance_transmit_byte(const spi_instance_t* dev, const uint8_t data)
{
    spi_write_blocking(dev->hw_inst, &data, 1);
}

void spi_instance_recieve(const spi_instance_t* dev, uint8_t* recieve_buffer, uint32_t length)
{
    spi_read_blocking(dev->hw_inst, 0, recieve_buffer, length);
}

uint8_t spi_instance_recieve_byte(const spi_instance_t* dev)
{
    uint8_t byte;
    spi_read_blocking(dev->hw_inst, 0, &byte, 1);
    return byte;
}

void spi_instance_transfer(const spi_instance_t* dev, const uint8_t* transmit_buffer, uint8_t* recieve_buffer, uint32_t length)
{
    spi_write_read_blocking(dev->hw_inst, transmit_buffer, recieve_buffer, length);
}