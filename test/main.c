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
 * Test for Software SPI module
 * 
 */

#include "sspi.h"
#include "unity.h"

#include <stdio.h>
#include <string.h>

/* Set to 1 if you want to see GPIO oscillograms */
#ifndef PRINT_SAMPLES
#define PRINT_SAMPLES 0
#endif

/*------------------------------------------------------------------------------------------------*/
/* GPIO pins */
/*------------------------------------------------------------------------------------------------*/
struct gpio_pin
{
    sspi_pin_state_t real;
    sspi_pin_state_t out;
    bool out_set;
    char const *in_samples;
    size_t samples_count;
    char samples[500];
};

/* Set default pin state */
static inline struct gpio_pin gpio_pin_new(void)
{
    return (struct gpio_pin){
        .real = SSPI_PIN_HIGH,
        .out = SSPI_PIN_HIGH,
        .in_samples = "",
    };
}

/* Get actual pin state */
static inline sspi_pin_state_t gpio_pin_read(struct gpio_pin *pin)
{
    return pin->real;
}

/* Change pin state from the MCU side */
static void gpio_pin_write(struct gpio_pin *pin, sspi_pin_state_t state)
{
    TEST_ASSERT(!pin->out_set);
    pin->out_set = true;
    pin->out = state;
}

/* Get actual oscillogram */
static inline char const *gpio_pin_get_samples(struct gpio_pin *pin)
{
    return pin->samples;
}

/* Set oscillogram of the connected external device */
static void gpio_pin_set_in(struct gpio_pin *pin,
                            char const *samples)
{
    TEST_ASSERT(samples);
    for (char const *c = samples; *c; c++)
    {
        TEST_ASSERT(*c == '_' || *c == '/' || *c == '^' || *c == '\\');
    }
    pin->in_samples = samples;
}

/* Save current GPIO state to the log */
static void gpio_pin_sample(struct gpio_pin *pin)
{
    TEST_ASSERT(pin->samples_count < sizeof(pin->samples));

    sspi_pin_state_t pin_in = (*pin->in_samples == '_' || *pin->in_samples == '\\') ?
                                  SSPI_PIN_LOW :
                                  SSPI_PIN_HIGH; /* Default state */
    if (*pin->in_samples) { pin->in_samples++; }

    /* Emulate open drain pins with pull up */
    sspi_pin_state_t new_real = (pin->out == SSPI_PIN_LOW || pin_in == SSPI_PIN_LOW) ?
                                    SSPI_PIN_LOW :
                                    SSPI_PIN_HIGH; /* Default state */

    pin->samples[pin->samples_count++] = (pin->real == new_real) ?
                                             (new_real ? '^' : '_') :
                                             (new_real ? '/' : '\\');
    pin->out_set = false;
    pin->real = new_real;
}
/*------------------------------------------------------------------------------------------------*/

/*------------------------------------------------------------------------------------------------*/
/* Software SPI implementation */
/*------------------------------------------------------------------------------------------------*/
static struct gpio_pin pin_sck, pin_miso, pin_mosi;

static void write_sck(struct sspi const *bus, sspi_pin_state_t state)
{
    gpio_pin_write(&pin_sck, state);
}

static void write_mosi(struct sspi const *bus, sspi_pin_state_t state)
{
    gpio_pin_write(&pin_mosi, state);
}

static sspi_pin_state_t read_miso(struct sspi const *bus)
{
    return gpio_pin_read(&pin_miso);
}

static void delay(struct sspi const *bus)
{
    gpio_pin_sample(&pin_sck);
    gpio_pin_sample(&pin_mosi);
    gpio_pin_sample(&pin_miso);
}
/*------------------------------------------------------------------------------------------------*/

/*------------------------------------------------------------------------------------------------*/
/* Unity hooks */
/*------------------------------------------------------------------------------------------------*/
void setUp(void)
{
    pin_sck = gpio_pin_new();
    pin_mosi = gpio_pin_new();
    pin_miso = gpio_pin_new();
}

void tearDown(void)
{
#if PRINT_SAMPLES
    printf("\nSCK:  %s", gpio_pin_get_samples(&pin_sck));
    printf("\nMOSI: %s", gpio_pin_get_samples(&pin_mosi));
    printf("\nMISO: %s", gpio_pin_get_samples(&pin_miso));
    printf("\n\n");
#endif
}
/*------------------------------------------------------------------------------------------------*/

/*------------------------------------------------------------------------------------------------*/
/* Tests */
/*------------------------------------------------------------------------------------------------*/
static void test_mode_0_msb_1bit(void)
{
    static struct sspi const sspi = {
        .write_sck = write_sck,
        .write_mosi = write_mosi,
        .read_miso = read_miso,
        .delay = delay,
        .word_size = 1,
    };

    gpio_pin_set_in(&pin_miso, "\\__/^^");

    /* Set pins to default state */
    sspi_reset(&sspi);
    sspi.delay(&sspi); /* Sample pins */

    /* Bits 2-7 are not used */
    uint8_t rd_buff[] = {0x00, 0x00};
    uint8_t wr_buff[] = {0x87, 0x5A}; /* Equals to {0x01, 0x00} */
    sspi_read_write(&sspi, rd_buff, wr_buff, sizeof(wr_buff));
    uint8_t rd_exp[] = {0x00, 0x01};
    TEST_ASSERT_EQUAL_UINT8_ARRAY(rd_exp, rd_buff, sizeof(rd_buff));

    /* Wait until pin levels are established (for tests only) */
    sspi.delay(&sspi); /* Sample pins */

    TEST_ASSERT_EQUAL_STRING("\\_/\\/\\",
                             gpio_pin_get_samples(&pin_sck));
    TEST_ASSERT_EQUAL_STRING("\\/^\\__",
                             gpio_pin_get_samples(&pin_mosi));
    TEST_ASSERT_EQUAL_STRING("\\__/^^",
                             gpio_pin_get_samples(&pin_miso));
}

static void test_mode_0_msb_5bit(void)
{
    static struct sspi const sspi = {
        .write_sck = write_sck,
        .write_mosi = write_mosi,
        .read_miso = read_miso,
        .delay = delay,
        .word_size = 5,
    };

    gpio_pin_set_in(&pin_miso, "\\/^^^\\_________/^\\_/^^");

    /* Set pins to default state */
    sspi_reset(&sspi);
    sspi.delay(&sspi); /* Sample pins */

    /* Bits 5-7 are not used */
    uint8_t rd_buff[] = {0x00, 0x00};
    uint8_t wr_buff[] = {0x87, 0x5A}; /* Equals to {0x07, 0x1A} */
    sspi_read_write(&sspi, rd_buff, wr_buff, sizeof(wr_buff));
    uint8_t rd_exp[] = {0x18, 0x05};
    TEST_ASSERT_EQUAL_UINT8_ARRAY(rd_exp, rd_buff, sizeof(rd_buff));

    /* Wait until pin levels are established (for tests only) */
    sspi.delay(&sspi); /* Sample pins */

    TEST_ASSERT_EQUAL_STRING("\\_/\\/\\/\\/\\/\\/\\/\\/\\/\\/\\",
                             gpio_pin_get_samples(&pin_sck));
    TEST_ASSERT_EQUAL_STRING("\\____/^^^^^^^^^\\_/^\\__",
                             gpio_pin_get_samples(&pin_mosi));
    TEST_ASSERT_EQUAL_STRING("\\/^^^\\_________/^\\_/^^",
                             gpio_pin_get_samples(&pin_miso));
}

static void test_mode_0_lsb_5bit(void)
{
    static struct sspi const sspi = {
        .write_sck = write_sck,
        .write_mosi = write_mosi,
        .read_miso = read_miso,
        .delay = delay,
        .lsb = true,
        .word_size = 5,
    };

    gpio_pin_set_in(&pin_miso, "\\______/^^^^^\\_/^\\____");

    /* Set pins to default state */
    sspi_reset(&sspi);
    sspi.delay(&sspi); /* Sample pins */

    /* Bits 5-7 are not used */
    uint8_t rd_buff[] = {0x00, 0x00};
    uint8_t wr_buff[] = {0x87, 0x5A}; /* Equals to {0x07, 0x1A} */
    sspi_read_write(&sspi, rd_buff, wr_buff, sizeof(wr_buff));
    uint8_t rd_exp[] = {0x18, 0x05};
    TEST_ASSERT_EQUAL_UINT8_ARRAY(rd_exp, rd_buff, sizeof(rd_buff));

    /* Wait until pin levels are established (for tests only) */
    sspi.delay(&sspi); /* Sample pins */

    TEST_ASSERT_EQUAL_STRING("\\_/\\/\\/\\/\\/\\/\\/\\/\\/\\/\\",
                             gpio_pin_get_samples(&pin_sck));
    TEST_ASSERT_EQUAL_STRING("\\/^^^^^\\_____/^\\_/^^^^",
                             gpio_pin_get_samples(&pin_mosi));
    TEST_ASSERT_EQUAL_STRING("\\______/^^^^^\\_/^\\____",
                             gpio_pin_get_samples(&pin_miso));
}

static void test_mode_0_msb_8bit(void)
{
    static struct sspi const sspi = {
        .write_sck = write_sck,
        .write_mosi = write_mosi,
        .read_miso = read_miso,
        .delay = delay,
    };

    gpio_pin_set_in(&pin_miso, "___/^^^^^^^\\_____/^\\_/^\\___/^\\_/^");

    /* Set pins to default state */
    sspi_reset(&sspi);
    sspi.delay(&sspi); /* Sample pins */

    uint8_t rd_buff[] = {0x00, 0x00};
    uint8_t wr_buff[] = {0x87, 0x5A};
    sspi_read_write(&sspi, rd_buff, wr_buff, sizeof(wr_buff));
    uint8_t rd_exp[] = {0x78, 0xA5};
    TEST_ASSERT_EQUAL_UINT8_ARRAY(rd_exp, rd_buff, sizeof(rd_buff));

    /* Wait until pin levels are established (for tests only) */
    sspi.delay(&sspi); /* Sample pins */

    TEST_ASSERT_EQUAL_STRING("\\_/\\/\\/\\/\\/\\/\\/\\/\\/\\/\\/\\/\\/\\/\\/\\/\\",
                             gpio_pin_get_samples(&pin_sck));
    TEST_ASSERT_EQUAL_STRING("\\/^\\_______/^^^^^\\_/^\\_/^^^\\_/^\\__",
                             gpio_pin_get_samples(&pin_mosi));
    TEST_ASSERT_EQUAL_STRING("\\__/^^^^^^^\\_____/^\\_/^\\___/^\\_/^^",
                             gpio_pin_get_samples(&pin_miso));
}

static void test_mode_2_msb_8bit(void)
{
    static struct sspi const sspi = {
        .write_sck = write_sck,
        .write_mosi = write_mosi,
        .read_miso = read_miso,
        .delay = delay,
        .cpol_1 = true,
    };

    gpio_pin_set_in(&pin_miso, "___/^^^^^^^\\_____/^\\_/^\\___/^\\_/^");

    /* Set pins to default state */
    sspi_reset(&sspi);
    sspi.delay(&sspi); /* Sample pins */

    uint8_t rd_buff[] = {0x00, 0x00};
    uint8_t wr_buff[] = {0x87, 0x5A};
    sspi_read_write(&sspi, rd_buff, wr_buff, sizeof(wr_buff));
    uint8_t rd_exp[] = {0x78, 0xA5};
    TEST_ASSERT_EQUAL_UINT8_ARRAY(rd_exp, rd_buff, sizeof(rd_buff));

    /* Wait until pin levels are established (for tests only) */
    sspi.delay(&sspi); /* Sample pins */

    TEST_ASSERT_EQUAL_STRING("^^\\/\\/\\/\\/\\/\\/\\/\\/\\/\\/\\/\\/\\/\\/\\/\\/",
                             gpio_pin_get_samples(&pin_sck));
    TEST_ASSERT_EQUAL_STRING("\\/^\\_______/^^^^^\\_/^\\_/^^^\\_/^\\__",
                             gpio_pin_get_samples(&pin_mosi));
    TEST_ASSERT_EQUAL_STRING("\\__/^^^^^^^\\_____/^\\_/^\\___/^\\_/^^",
                             gpio_pin_get_samples(&pin_miso));
}

static void test_mode_1_msb_8bit(void)
{
    static struct sspi const sspi = {
        .write_sck = write_sck,
        .write_mosi = write_mosi,
        .read_miso = read_miso,
        .delay = delay,
        .cpha_1 = true,
    };

    gpio_pin_set_in(&pin_miso, "\\___/^^^^^^^\\_____/^\\_/^\\___/^\\_/^");

    /* Set pins to default state */
    sspi_reset(&sspi);
    sspi.delay(&sspi); /* Sample pins */

    uint8_t rd_buff[] = {0x00, 0x00};
    uint8_t wr_buff[] = {0x87, 0x5A};
    sspi_read_write(&sspi, rd_buff, wr_buff, sizeof(wr_buff));
    uint8_t rd_exp[] = {0x78, 0xA5};
    TEST_ASSERT_EQUAL_UINT8_ARRAY(rd_exp, rd_buff, sizeof(rd_buff));

    /* Wait until pin levels are established (for tests only) */
    sspi.delay(&sspi); /* Sample pins */

    TEST_ASSERT_EQUAL_STRING("\\_/\\/\\/\\/\\/\\/\\/\\/\\/\\/\\/\\/\\/\\/\\/\\/\\",
                             gpio_pin_get_samples(&pin_sck));
    TEST_ASSERT_EQUAL_STRING("\\_/^\\_______/^^^^^\\_/^\\_/^^^\\_/^\\_",
                             gpio_pin_get_samples(&pin_mosi));
    TEST_ASSERT_EQUAL_STRING("\\___/^^^^^^^\\_____/^\\_/^\\___/^\\_/^",
                             gpio_pin_get_samples(&pin_miso));
}

static void test_mode_3_msb_8bit(void)
{
    static struct sspi const sspi = {
        .write_sck = write_sck,
        .write_mosi = write_mosi,
        .read_miso = read_miso,
        .delay = delay,
        .cpol_1 = true,
        .cpha_1 = true,
    };

    gpio_pin_set_in(&pin_miso, "\\___/^^^^^^^\\_____/^\\_/^\\___/^\\_/^");

    /* Set pins to default state */
    sspi_reset(&sspi);
    sspi.delay(&sspi); /* Sample pins */

    uint8_t rd_buff[] = {0x00, 0x00};
    uint8_t wr_buff[] = {0x87, 0x5A};
    sspi_read_write(&sspi, rd_buff, wr_buff, sizeof(wr_buff));
    uint8_t rd_exp[] = {0x78, 0xA5};
    TEST_ASSERT_EQUAL_UINT8_ARRAY(rd_exp, rd_buff, sizeof(rd_buff));

    /* Wait until pin levels are established (for tests only) */
    sspi.delay(&sspi); /* Sample pins */

    TEST_ASSERT_EQUAL_STRING("^^\\/\\/\\/\\/\\/\\/\\/\\/\\/\\/\\/\\/\\/\\/\\/\\/",
                             gpio_pin_get_samples(&pin_sck));
    TEST_ASSERT_EQUAL_STRING("\\_/^\\_______/^^^^^\\_/^\\_/^^^\\_/^\\_",
                             gpio_pin_get_samples(&pin_mosi));
    TEST_ASSERT_EQUAL_STRING("\\___/^^^^^^^\\_____/^\\_/^\\___/^\\_/^",
                             gpio_pin_get_samples(&pin_miso));
}

/* The test shows how to increase the word size to 9 bits or more. */
static void test_mode_0_10bits(void)
{
    static struct sspi const sspi = {
        .write_sck = write_sck,
        .write_mosi = write_mosi,
        .read_miso = read_miso,
        .delay = delay,
    };

    gpio_pin_set_in(&pin_miso, "\\__/^^^^^^^\\_____/^\\__");

    /* Set pins to default state */
    sspi_reset(&sspi);
    sspi.delay(&sspi); /* Sample pins */

    /* Send 10 bits data in MSB format */
    uint16_t wr = 0x021D;
    uint16_t rd = (sspi_byte_read_write(&sspi, wr >> 2 & 0xFF)) << 2 |          /* Bits 9-2 */
                  ((sspi_bit_read_write(&sspi, wr >> 1 & 0x01)) ? 1 : 0) << 1 | /* Bit 1 */
                  ((sspi_bit_read_write(&sspi, wr >> 0 & 0x01)) ? 1 : 0) << 0;  /* Bit 0 */

    TEST_ASSERT_EQUAL_UINT16(0x01E2, rd);

    /* Wait until pin levels are established (for tests only) */
    sspi.delay(&sspi); /* Sample pins */

    TEST_ASSERT_EQUAL_STRING("\\_/\\/\\/\\/\\/\\/\\/\\/\\/\\/\\",
                             gpio_pin_get_samples(&pin_sck));
    TEST_ASSERT_EQUAL_STRING("\\/^\\_______/^^^^^\\_/^^",
                             gpio_pin_get_samples(&pin_mosi));
    TEST_ASSERT_EQUAL_STRING("\\__/^^^^^^^\\_____/^\\__",
                             gpio_pin_get_samples(&pin_miso));
}
/*------------------------------------------------------------------------------------------------*/

/*------------------------------------------------------------------------------------------------*/
int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_mode_0_msb_1bit);
    RUN_TEST(test_mode_0_msb_5bit);
    RUN_TEST(test_mode_0_lsb_5bit);
    RUN_TEST(test_mode_0_msb_8bit);
    RUN_TEST(test_mode_2_msb_8bit);
    RUN_TEST(test_mode_1_msb_8bit);
    RUN_TEST(test_mode_3_msb_8bit);
    RUN_TEST(test_mode_0_10bits);
    return UNITY_END();
}
/*------------------------------------------------------------------------------------------------*/