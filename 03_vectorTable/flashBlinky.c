#include <stdint.h>
#include <stdbool.h>

// Define necessary register addresses
#define RESETS_RESET                                    *(volatile uint32_t *) (0x4000c000)
#define RESETS_RESET_DONE                               *(volatile uint32_t *) (0x4000c008)
#define IO_BANK0_GPIO25_CTRL                            *(volatile uint32_t *) (0x400140cc)
#define SIO_GPIO_OE_SET                                 *(volatile uint32_t *) (0xd0000024)
#define SIO_GPIO_OUT_XOR                                *(volatile uint32_t *) (0xd000001c)

// Main entry point
int main(void)
{
    RESETS_RESET &= ~(1 << 5); // Bring IO_BANK0 out of reset state
    while (!(RESETS_RESET_DONE & (1 << 5))); // Wait for peripheral to respond
    IO_BANK0_GPIO25_CTRL = 5; // Set GPIO 25 function to SIO
    SIO_GPIO_OE_SET |= 1 << 25; // Set output enable for GPIO 25 in SIO

    while (true)
    {
        for (uint32_t i = 0; i < 100000; ++i); // Wait for some time
        SIO_GPIO_OUT_XOR |= 1 << 25;  // Flip output for GPIO 25
    }
}