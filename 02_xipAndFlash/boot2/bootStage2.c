#include <stdint.h>
#include <stdbool.h>

// Define necessary register addresses
// XIP
#define XIP_BASE                    (0x10000000)
// SSI
#define SSI_BASE                    (0x18000000)
#define SSI_CTRLR0                  (*(volatile uint32_t *) (SSI_BASE + 0x000))
#define SSI_SSIENR                  (*(volatile uint32_t *) (SSI_BASE + 0x008))
#define SSI_BAUDR                   (*(volatile uint32_t *) (SSI_BASE + 0x014))
#define SSI_SPI_CTRLR0              (*(volatile uint32_t *) (SSI_BASE + 0x0f4))

// A brief list of steps to take
// 1. Setup IO_QSPI pins for XIP
// 2. Setup SSI interface
// 3. Enable XIP Cache

// Boot stage 2 entry point
__attribute__((section(".boot2"))) void bootStage2(void)
{
    // 1. Setup IO_QSPI pins for XIP (already done by bootrom)
    //  - Bring IO_QSPI out of reset state
    //  - Set SCLK and SS to OE
    //  - Set all IO_QSPI GPIOs function to XIP

    // 2. Setup SSI interface
    SSI_SSIENR = 0; // Disable SSI to configure it
    SSI_BAUDR = 4; // Set clock divider to 4
    SSI_CTRLR0 = (3 << 8) | (31 << 16); // Set EEPROM mode and 32 clocks per data frame
    SSI_SPI_CTRLR0 = (6 << 2) | (2 << 8) | (0x03 << 24); // Set address length to 24-bits, instruction length to 8-bits and command to Read Data (03h)
    SSI_SSIENR = 1; // Enable SSI

    // 3. Enable XIP Cache
    // It is enabled by default. Take a look at https://datasheets.raspberrypi.com/rp2040/rp2040-datasheet.pdf#page=128

    void (*main)(void) = (void (*)())0x10000101; // Note 0x10000100 + 0x1 (this forces the instructions to be interpreted as thumb)
    main();

    // Just to be safe if we come back here
    while(true);
}