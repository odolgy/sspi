/*
 * Copyright (c) 2020 Oleg Dolgy
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 * 
 * Software implementation of Serial Peripheral Interface
 * 
 */

#ifndef SOFTBUS_SSPI_H
#define SOFTBUS_SSPI_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* GPIO pin state */
typedef enum
{
    SSPI_PIN_LOW = 0,
    SSPI_PIN_HIGH,
} sspi_pin_state_t;

/* Software SPI bus handle */
struct sspi
{
    /* Set state of the SCK pin */
    void (*write_sck)(struct sspi const *bus, sspi_pin_state_t state);
    /* Set state of the MOSI pin */
    void (*write_mosi)(struct sspi const *bus, sspi_pin_state_t state);
    /* Get state of the MISO pin */
    sspi_pin_state_t (*read_miso)(struct sspi const *bus);
    /* Wait for a period equals to the half period of the clock frequency */
    void (*delay)(struct sspi const *bus);
    /* Clock polarity: 1 (true) or 0 (false).
     * When CPOL is 0, the leading edge of the SCK is a low to high transition 
     * and the trailing edge is a high to low transition: __/^\__.
     * When CPOL is 1, the leading edge of the SCK is a high to low transition
     * and the trailing edge is a low to high transition: ^^\_/^^.
     * */
    bool cpol_1;
    /* Clock phase: 1 (true) or 0 (false).
     * When CPHA is 0, the data is sampled on the leading edge and 
     * is changed on the trailing edge.
     * When CPHA is 1, the data is sampled on the trailing edge and 
     * is changed on the leading edge.
     * */
    bool cpha_1;
    /* Bits ordering: LSB (true) or MSB (false) */
    bool lsb;
    /* Word size in bits: 1-8.
     * Values outside interval [1,7] correspond to the default word_size size: 8 bits.
     * When the word_size size is lower than 8 bits, the lowest bits of the byte are used.
     * For example, when the 'word_size' equals 5 and you want to send all 'ones', use value 0x1F.
     * */
    int word_size;
};

/* Set SCK and MOSI pins to default state.
 * Optionally you may use it: 
 * - after GPIO initialization to make sure the SCK level matches the CPOL setting;
 * - after write operations to make sure the MOSI level has been returned to 0. 
 * */
static inline void sspi_reset(struct sspi const *bus)
{
    bus->write_sck(bus, bus->cpol_1 ? SSPI_PIN_HIGH : SSPI_PIN_LOW);
    bus->write_mosi(bus, SSPI_PIN_LOW);
}

/* Read and write one bit */
static inline sspi_pin_state_t sspi_bit_read_write(struct sspi const *bus, sspi_pin_state_t write_bit)
{
    sspi_pin_state_t const sck_lead = bus->cpol_1 ? SSPI_PIN_LOW : SSPI_PIN_HIGH;
    sspi_pin_state_t const sck_trail = bus->cpol_1 ? SSPI_PIN_HIGH : SSPI_PIN_LOW;
    sspi_pin_state_t read_bit;

    if (bus->cpha_1)
    {
        bus->delay(bus);

        /* Write bit on the leading edge */
        bus->write_sck(bus, sck_lead);
        bus->write_mosi(bus, write_bit);
        bus->delay(bus);

        /* Read bit on the trailing edge */
        bus->write_sck(bus, sck_trail);
        read_bit = bus->read_miso(bus);
    }
    else
    {
        /* Write bit */
        bus->write_mosi(bus, write_bit);
        bus->delay(bus);

        /* Read bit on the leading edge */
        bus->write_sck(bus, sck_lead);
        read_bit = bus->read_miso(bus);
        bus->delay(bus);

        /* Trailing edge */
        bus->write_sck(bus, sck_trail);
    }

    return read_bit;
}

/* Read and write one byte */
uint8_t sspi_byte_read_write(struct sspi const *bus, uint8_t write_byte);

/* Bidirectional read/write operation.
 * Use a NULL pointer for read_buff or write_buff if you need one-directional operation. */
void sspi_read_write(struct sspi const *bus,
                     uint8_t *read_buff,
                     uint8_t const *write_buff,
                     size_t size);

/* Read data array */
static inline void sspi_read(struct sspi const *bus,
                             uint8_t *read_buff,
                             size_t size)
{
    sspi_read_write(bus, read_buff, NULL, size);
}

/* Write data array */
static inline void sspi_write(struct sspi const *bus,
                              uint8_t const *write_buff,
                              size_t size)
{
    sspi_read_write(bus, NULL, write_buff, size);
}

#endif /* SOFTBUS_SSPI_H */
