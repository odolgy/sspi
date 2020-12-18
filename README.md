# SSPI
SSPI is a cross-platform software SPI driver for master device.

## Why to use software driver?
The benefits of using hardware-based drivers are undeniable. But in some cases this is not possible:
- There are no free pins controlled by the driver;
- Peripheral device was connected to wrong pins due to schematic mistake;
- The operating mode required for the peripheral device is not supported by the driver.

But that doesn't mean you should stop experimenting and prototyping. Software driver based on GPIO pins is the solution you need.

## Why to use SSPI library?
There are some good platform-specific libraries like [SoftSPI](https://github.com/MajenkoLibraries/SoftSPI) for Arduino. But SSPI is written in plain C and can be used on any platform. It is **not the fastest solution**, but you can easily run it on any MCU.

The code is covered with unit tests and the result is predictable. You can see the expected SCK, MOSI and MISO oscillograms by cloning the repo and running tests (see "test/main.c" and "test/Makefile").

Supported features:
- All SPI modes (0-3) determined by CPOL and CPHA settings;
- Configurable bit ordering: MSB and LSB;
- Configurable word length for complex read/write operations: from 1 to 8 bits;
- Access to low-level read and write operations of bits and bytes to create special operations (e.g. increasing the word size to 9 bits or higher);

## How to use
1. Configure pins: SCK and MOSI as Push-Pull outputs and MISO as an input. The default level of SCK pin depends on CPOL setting.
2. Connect SSPI driver to the hardware platform by declaring these functions:
```
void write_sck(struct sspi const *bus, sspi_pin_state_t state)
{
    // Write SCK pin here
}

void write_mosi(struct sspi const *bus, sspi_pin_state_t state)
{
    // Write MOSI pin here
}

sspi_pin_state_t read_miso(struct sspi const *bus)
{
    // Read MISO pin here
}

void delay(struct sspi const *bus)
{
    // Wait SPI_CLOCK_FREQ / 2
}
```
3. Declare sspi structure with desired configuration:
```
struct sspi const sspi = {
    .write_sck = write_sck,
    .write_mosi = write_mosi,
    .read_miso = read_miso,
    .delay = delay,
    .cpol_1 = true,
    .cpha_1 = true,
    .lsb = true,
    .word_size = 4,
};
```
4. Communicate with peripheral devices using the functions in "sspi.h". Note that the Slave Select (or Chip Select) pin must be controlled in the user code.
