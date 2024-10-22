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
#define SSI_SR                      (*(volatile uint32_t *) (SSI_BASE + 0x028))
#define SSI_DR0                     (*(volatile uint32_t *) (SSI_BASE + 0x060))
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
    //  - Read 2 byte status register
    SSI_SSIENR = 0; // Disable SSI to configure it
    SSI_BAUDR = 4; // Set clock divider to 4
    SSI_CTRLR0 = (7 << 16); // Set 8 clocks per data frame
    SSI_SSIENR = 1; // Enable SSI
    SSI_DR0 = 0x05; // Read Status Register 1
    SSI_DR0 = 0x35; // Read Status Register 2
    while ((SSI_SR & (1 << 2)) && (~SSI_SR & 1)); // Wait while Transmit FIFO becomes empty and SSI is not busy
    uint8_t stReg1 = SSI_DR0; // Copy Status Register 1 value
    uint8_t stReg2 = SSI_DR0; // Copy Status Register 2 value
    //  - If QE bit is not set, then set QE bit
    if (!(stReg2 & 1 << 1))
    {
        SSI_DR0 = 0x06; // Execute Write Enable Instruction
        SSI_DR0 = 0x01; // Execute Write Status Register Instruction
        SSI_DR0 = stReg1; // Write Status Register 1 value
        SSI_DR0 = stReg2 | 1 << 1; // Status Register 2 value
        SSI_DR0 = 0x04; // Execute Write Disable Instruction
    }

    //  - Configure SSI for Fast Read Quad IO Instruction (EBh)
    SSI_SSIENR = 0; // Disable SSI to configure it
    SSI_CTRLR0 = (3 << 8) | (31 << 16) | (2 << 21); // Set SPI frame format to 0x2, EEPROM mode and 32 clocks per data frame
    // SSI_CTRLR1 = 0;
    SSI_SPI_CTRLR0 = (8 << 2) | (2 << 8) | (4 << 11) | (0x1 << 0); // Set address length to 32-bits (Address + Mode), instruction length to 8-bits, set wait cycles to 4, and  Command in standard SPI and address in format specified by FRF
    SSI_SSIENR = 1; // Enable SSI
    //  - Perform a dummy read with mode bits set to 0xa0 to avoid sending the instruction again
    SSI_DR0 = 0xeb; // Send Fast Read Quad I/O Instruction
    SSI_DR0 = 0xa0; // Send address (0x0) + mode (0xa0) = 0b00000000000000000000000010100000
    while ((SSI_SR & (1 << 2)) && (~SSI_SR & 1)); // Wait while Transmit FIFO becomes empty and SSI is not busy
    // while ((SSI_SR & (1 << 2)) && (~SSI_SR & 1)); // Wait while Transmit FIFO becomes empty and SSI is not busy
    //  - Configure SSI for sending the address and mode bits only
    SSI_SSIENR = 0; // Disable SSI to configure it
    // SSI_CTRLR0 = (3 << 8) | (31 << 16) | (2 << 21); // Set SPI frame format to 0x2, EEPROM mode and 32 clocks per data frame
    SSI_SPI_CTRLR0 = (8 << 2) | (0 << 8) | (0xa0 << 24) | (4 << 11) | (0x2 << 0); // Set address length to 32-bits (Address + Mode), mode bits to append to address (0xa0), set wait cycles to 4, and  Command and address in format specified by FRF
    SSI_SSIENR = 1; // Enable SSI

    // 3. Enable XIP Cache
    // It is enabled by default. Take a look at https://datasheets.raspberrypi.com/rp2040/rp2040-datasheet.pdf#page=128

    void (*main)(void) = (void (*)())0x10000101; // Note 0x10000100 + 0x1 (this forces the instructions to be interpreted as thumb)
    main();

    // Just to be safe if we come back here
    while(true);
}