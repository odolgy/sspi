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

#include "sspi.h"

uint8_t sspi_byte_read_write(struct sspi const *bus, uint8_t write_byte)
{
    int const word_size = (bus->word_size && bus->word_size < 8) ? bus->word_size : 8;
    uint8_t const word_msb_mask = 1 << (word_size - 1);
    uint8_t const mask_tx = bus->lsb ? 0x01 : word_msb_mask;
    uint8_t const mask_rx = bus->lsb ? word_msb_mask : 0x01;
    uint8_t read_byte = 0;

    for (int bit = 0; bit < word_size; bit++)
    {
        read_byte |= sspi_bit_read_write(bus, write_byte & mask_tx) ? mask_rx : 0x00;

        if (bit < word_size - 1)
        {
            if (bus->lsb)
            {
                write_byte >>= 1;
                read_byte >>= 1;
            }
            else
            {
                write_byte <<= 1;
                read_byte <<= 1;
            }
        }
    }

    return read_byte;
}

void sspi_read_write(struct sspi const *bus,
                     uint8_t *read_buff,
                     uint8_t const *write_buff,
                     size_t size)
{
    while (size--)
    {
        uint8_t const tx_byte = write_buff ? *write_buff++ : 0x00;
        uint8_t const rx_byte = sspi_byte_read_write(bus, tx_byte);
        if (read_buff) { *read_buff++ = rx_byte; }
    }
}