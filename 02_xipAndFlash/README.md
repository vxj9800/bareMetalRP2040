## 2. Execute Code from Flash 
In the last chapter, you learned the boot-up process of RP2040 &micro;C. In summary, the process is relatively simple, the `bootrom` verifies that the first 256 Bytes in Flash is valid, loads it into RAM and start executing it. But, the RP2040 can work with any Flash chip that can communicate over SPI protocol and is smaller than 16 MBytes in size. The development board used in these tutorials (Pi Pico) has 2 MBytes of Flash. It also means that it should be able to execute a program that is larger than 256 Bytes.

RP2040 contains XIP (Execute-In-Place) peripheral to allow processor to fetch instructions directly from Flash. The idea is simple, convert the read accesses requests from the processor into SPI commands. Thus, XIP acts as a Bus-To-SPI translation layer between the RP2040's processor and a Flash that can communicate over SPI. The goal of this chapter is to setup XIP such that the code can be executed directly from Flash. In fact, the 256 Bytes code that is loaded to RAM by `bootrom` and executed, is technically supposed to configure XIP, this is by design.

### SPI Protocol
Let's first brush-up on Serial Peripheral Interface (SPI) before discussing its variants Double-SPI and Quad-SPI.

SPI was originally developed by Motorola in 1980s. It is used heavily in today's time for communication between two &micro;Cs, a &micro;C and sensor/s or actuator/s. There are may flavors of SPI as well, like Microwire (developed by National Semiconductor) and Synchronous Serial Protocol (developed by Texas Instruments). Motorola's version of SPI is discussed here.

Following image shows the connections between a &micro;C and a sensor that can communicate through SPI protocol.
![image](../misc/figs/chap2/connectPicoSPI.svg)

The important pin names are,
1. `CSn` - Active low chip-select or slave-select, AKA: `CS`, `/SS`, <code><span style="text-decoration:overline">CS</span></code>, `SSn`.
2. `SCK` - Clock signal, AKA: `CLK`, `SCLK`
3. `TX` - Data Transmit Line, AKA: `SDO`, `DO`, `MOSI` (Master-Out-Slave-In on &micro;C side), `MISO` (Master-In-Slave-Out on sensor side)
4. `RX` - Data Receive Line, AKA: `SDI`, `DI`, `MOSI` (Master-Out-Slave-In on sensor side), `MISO` (Master-In-Slave-Out on &micro;C side)

There are four modes over which SPI can work. These modes are just combinations of clock polarity (idle state low or high) and clock phase (data sample on rising or falling edge). The discussion here assumes mode 0, which corresponds to idle clock state being logic low and data being sampled on the rising edge of the clock.

Following diagram shows a common data write message.
![image](../misc/figs/chap2/commonSpiWrite.svg)

Following diagram shows a common data read message.
![image](../misc/figs/chap2/commonSpiRead.svg)

You might have achieved something similar to what is shown in the diagrams above if you have worked with a sensor before that communicates over SPI protocol. Our goal here is to use faster variants of SPI protocol so that the processor can fetch the instructions from the Flash chip available onboard. Following reading material about XIP, SSI and Flash will be helpful in upcoming sections.
- [RP2040 Datasheet, Section 4.10 SSI](https://datasheets.raspberrypi.com/rp2040/rp2040-datasheet.pdf#page=572)
- [W25Q80DV Flash Datasheet, Chapters 6-8](https://www.winbond.com/resource-files/w25q80dv%20dl_revh_10022015.pdf#page=11)

### Let's Start Slow
Reading the Flash datasheet, you'll very quickly learn, in Chapter 6, that it can work with three variants of SPI; Standard SPI, Dual-SPI and Quad-SPI; in modes 0 and 3. Let's stick to Standard SPI in mode 0 for now. It also contains registers like `CONTROL` and `STATUS` that would require special attention. With the goal of reading the instructions from Flash in mind, you'd soon realize that you just have to make the &micro;C spit out the [*Read Data* (`0x3`)](https://www.winbond.com/resource-files/w25q80dv%20dl_revh_10022015.pdf#page=26) instruction along with the appropriate address and the Flash will return the program instructions stored at that address. Following diagram shows the SPI message associated with such an interaction.
![image](../misc/figs/chap2/flashReadData.svg)

Unfortunately, the discussion in RP2040's Datasheet is not that easy to follow. Regardless, on the &micro;C's side, you'd notice that almost every important aspect of this communication is handled by three registers,
1. [Control Register 0 (`CTRLR0`)](https://datasheets.raspberrypi.com/rp2040/rp2040-datasheet.pdf#page=603)
2. [Baud rate (`BAUDR`)](https://datasheets.raspberrypi.com/rp2040/rp2040-datasheet.pdf#page=604)
3. [SPI control (`SPI_CTRLR0`)](https://datasheets.raspberrypi.com/rp2040/rp2040-datasheet.pdf#page=609)

The discussion of `BAUDR`'s value is provided in [Section 4.10.4 of RP2040 Datasheet](https://datasheets.raspberrypi.com/rp2040/rp2040-datasheet.pdf#page=575). It mentions that the frequency of the SPI CLK signal is the function of $`f_{ssi\_clk}`$ and the 16-bit value (`SCKDV`) in `BAUDR` register. Also note that only an even value in range (0, 65534] is allowed. Thus, the `BAUDR` register's value can be figured out if the desired SPI CLK frequency is known. This information is available in [Section 9.6 of Flash Datasheet](https://www.winbond.com/resource-files/w25q80dv%20dl_revh_10022015.pdf#page=59). It states that the maximum SPI clock frequency is 33 MHz for Read Data Instruction (0x3). Thus, the `SCKDV` value in `BAUDR` register is,
```math
\texttt{SCKDV} = \frac{f_{ssi\_clk}}{f_{sclk\_out}} = \frac{125 MHz}{33 MHz} = 3.7878 \approx 4 \qquad \because f_{ssi\_clk} = f_{clk\_sys}
```

Let's look at different bits of `CTRLR0` and figure out what should go in each,
- `DFS` - In [Section 4.10.6 of RP2040 Datasheet](https://datasheets.raspberrypi.com/rp2040/rp2040-datasheet.pdf#page=576) that the `DFS` (`CTRLR0[3:0]`) is invalid and writing to it has no effect. So, it is left unchanged. 
- `FRF` - On [pg. 593 of RP2040 Datasheet](https://datasheets.raspberrypi.com/rp2040/rp2040-datasheet.pdf#page=593) it is mentioned that `CTRLR0.SPI_FRF` is only applicable if `FRF` is programmed to `0b00`. So, it is left unchanged.
- `SCPH` and `SCPOL` - These two bits specify the SPI mode. As mentioned previously, the discussion here assumes mode 0. Thus, both the bits are left unchanged. Further discussion on this is available in [Section 4.10.10.1 of RP2040 Datasheet](https://datasheets.raspberrypi.com/rp2040/rp2040-datasheet.pdf#page=584).
- `TMOD` - These bits are set to `0x3`, EEPROM read mode (TX then RX; RX starts after control data TX'd), since for every transaction a command (instruction + address) is transmitted and the data is received right after.
- `SLV_OE` - Left unchanged since the &micro;C is supposed to act as a master in this case.
- `SRL` - Left unchanged since enough information about this bit is not available.
- `CFS` - Left unchanged since these bits correspond to Microwire flavor of SPI protocol, which is not being used here.
- `DFS_32` - Set to 31 since the data from Flash is expected to be 32-bits in size.
- `SPI_FRF` - Left unchanged, to `0x0`, since the communication is supposed to happen in Standard SPI format, not in Dual-SPI or Quad-SPI format.
- `SSTE` - Enabling this will make the `CSn` to go high at the end of a message. The Flash used here doesn't mandate this, so it is left unchanged, to `0x0`.

Let's look at the XIP related configuration in `SPI_CTRLR0`,
- `TRANS_TYPE` - Allows the choice of different parts of the message (instruction, address or data) to be sent in format specified by `SPI_FRF` in `CTRLR0`. Since current discussion relates to Standard SPI format, this field is left to `0x0.`
- `ADDR_L` - The addresses in Flash are 24-bits in size. Note that the field description in the datasheet mentions that the value should be in 4-bit increments. Thus, this field is set to `24 / 4 = 6`.
- `INST_L` - All the instructions of Flash chip used here are 8-bits long. So, this field is set to `0x2`.
- `WAIT_CYCLES` - Left unchanged here. Makes SSI to add dummy SCLK cycles between command and data. Can be useful for Flash instructions similar to Fast Read (`0xB`).
- `SPI_DDR_EN` - Left unchanged since SPI is not operating in DDR mode.
- `INST_DDR_EN` - Left unchanged since SPI is not operating in DDR mode.
- `SPI_RXDS_EN` - Left unchanged since enough information about this bit is not available.
- `XIP_CMD` - Set to `0x3` (Read Data Instruction) because this needs to be appended before 24-bit address as shown in the diagram [above](#lets-start-slow).

### Convert Into a Program
To put everything discussed so far into a working firmware, let's first copy the content of last tutorial into a new directory and make the following changes.
1. Rename `boot2Blinky.c` to `flashBlinky.c`.
2. Replace `__attribute__((section(".boot2"))) void bootStage2(void)` with `int main(void)` in `flashBlinky.c`.
3. Move `compCrc32.cpp` into a sub-folder called `boot2`.
4. Create a new file called `bootStage2.c` in `boot2` sub-folder.

The complete process can now be described as follows,
1. `bootrom` checks whether first 256 Bytes (which contains `bootStage2` function) of Flash is valid or not.
2. If it is, then `bootrom` loads those 256 Bytes into RAM and executes it.
3. The `bootStage2` function sets up XIP peripheral, to allow processor to fetch instructions directly from Flash, and then jumps to the `main` function.

Consider the following C code, which would go in the new `bootStage2.c` file.
```C {.numberLines}
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
    // 1. Setup IO_QSPI pins for XIP (already done by `bootrom`)
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
```
Note that Section 2 of the code above first disables SSI peripheral, changes register values to what was discussed in [previous section](#lets-start-slow) and enables SSI peripheral. This concludes the setup of XIP peripheral. Now the processor should be able to fetch instructions directly from Flash.

Section 3 of the code above is calling the `main` function defined in `flashBlinky.c` file. To do this, the address of `main` function (`0x10000101`) is converted into a function pointer (`(void (*)())`) and then the `main` function is actually called. The address of the `main` function is known precisely to be `0x10000100` since it will be placed right after the `.crc` section, with the help of the Linker Script. The difference of `0x1` in the address tells the processor to interpret the instructions as [Thumb and not Arm](https://stackoverflow.com/questions/28669905/what-is-the-difference-between-the-arm-thumb-and-thumb-2-instruction-encodings).

Make following changes to the linker script to put `main` function right after `.crc` section in the binary.
```ld {.numberLines}
ENTRY(bootStage2);

MEMORY
{
    flash(rx) : ORIGIN = 0x10000000, LENGTH = 2048k
    sram(rwx) : ORIGIN = 0x20000000, LENGTH = 256k
}

SECTIONS
{
    .boot2 :                                    /* Changed */
    {
        _sboot2 = .;
        *(.boot2*)                              /* bootStage2 function */
        _eboot2 = .;
        . = . + (252 - (_eboot2 - _sboot2));    /* Pad zeros, addCounter = addCounter + (252 - sizeOfBoot2) */
        *(.crc*)                                /* 4 Byte CRC32 value */
    } > flash

    .text :                                     /* Added */
    {                                           /* Added */
        *(.text*)                               /* Added */
    } > flash                                   /* Added */
}
```
The only major change here compared to last tutorial is that the first 256 Bytes (`.boot2` and `.crc` sections) are now placed in a separate `.boot2` section. However, there are some interesting things happening in that section that needs some explanation. In a linker script, a `.` represents the current address counter. As more code/sections are added, the address counter keeps increasing based on the size of each code/section. You can also defined variables containing the value this address counter at specific locations. That's exactly what's happening in case of `_sboot2 = .` and `_eboot2 = .`. These two variables define starting and ending addresses of the `.boot2` section. These two variables are then used to pad the remaining space of first 252 Bytes with zeros in line `. = . + (252 - (_eboot2 - _sboot2));`. And finally, the `.crc` section is appended at the end.

> [!NOTE]  
> Significant changes are done in the `Makefile`. So, take a look at its content and make sure you understand what commands the `Makefile` is executing.

Now you should be able to generate a `*.uf2` file, upload it on the &micro;C and see the LED flashing, which is handled by the `main` function that was never loaded into RAM.

### Switching Gears
Hold on, the eventual goal of this tutorial was to use faster variants of SPI protocol right!? So, let's give Quad-SPI a shot. The high speed read instructions mentioned in [Flash documentation](https://www.winbond.com/resource-files/w25q80dv%20dl_revh_10022015.pdf#page=20) are [Fast Read Quad Output (`0x6B`)](https://www.winbond.com/resource-files/w25q80dv%20dl_revh_10022015.pdf#page=29) and [Fast Read Quad IO (`0xEB`)](https://www.winbond.com/resource-files/w25q80dv%20dl_revh_10022015.pdf#page=31). But to make use of these instructions, the Flash needs to be put into Quad-SPI mode by setting Quad Enable (QE) bit in [Status Register 2](https://www.winbond.com/resource-files/w25q80dv%20dl_revh_10022015.pdf#page=15). And, the [Write Status Register (`0x01`)](https://www.winbond.com/resource-files/w25q80dv%20dl_revh_10022015.pdf#page=25) has to be used to change the value of a status register, along with [Write Enable (`0x06`)](https://www.winbond.com/resource-files/w25q80dv%20dl_revh_10022015.pdf#page=22) and [Write Disable (`0x04`)](https://www.winbond.com/resource-files/w25q80dv%20dl_revh_10022015.pdf#page=23) instructions.

There are two registers on the &micro;C, [Status Register (`SR`)](https://datasheets.raspberrypi.com/rp2040/rp2040-datasheet.pdf#page=602) and [Data Register (`DR0`)](https://datasheets.raspberrypi.com/rp2040/rp2040-datasheet.pdf#page=605), that will aid in putting the Flash into Quad-SPI mode. Two bits of the Status Register, BUSY (0th-bit) and TFE (2nd-bit), indicate whether the SSI is busy and whether the data was transmitted or not. The Data register, in reality, writes or reads data, to or from, transmit or receive FIFOs of SSI, when the register is written to or read from. Consider the following piece of code that makes use of these two registers to put the Flash into Quad-SPI mode,
```C
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
```
Note that Section 2 of the code above makes SSI to expect 8-bit data frame for now. After sending instructions for reading Status Registers 1 and 2 from Flash, the Status Register in &micro;C is used to make sure that the transaction was finished before reading the responses from Flash by accessing Data Register. Then the QE bit in Flash Status Register 2 is checked to determine whether it needs to be updated or not. To update it's value, first the Write Enable (`0x06`) instruction is transmitted followed by the Write Status Register (`0x01`) instruction, followed by the desired values of the two status registers and finally the Write Disable (`0x04`) instruction. Nice, the Flash is now in Quad-SPI mode.

But wait, what is Quad-SPI mode? Let's first take a look at the real connection diagram between RP2040 and W25Q80DV Flash chip before answering this question.

![img](../misc/figs/chap2/connectPicoFlash.svg)

Note that instead of the standard four connections, `CSn`, `SCK`, `TX` and `RX`, between the &micro;C and the Flash, there are actually six connections. Furthermore, the data lines on the &micro;C side is now labelled as `SD0`, `SD1`, `SD2` and `SD3` instead of `TX` and `RX`. This defines the Quad-SPI mode, where the data is communicated over four data lines. When the Flash is put into Quad-SPI mode, the `RX`, `TX`, `WPn` and `HOLDn` pins are repurposed to work as `SD0`, `SD1`, `SD2` and `SD3`. Following diagram shows the Quad-SPI format message associated with Fast Read Quad Output (`0x6B`) instruction.

![img](../misc/figs/chap2/flashReadQuadOut.svg)

Let's do a quick comparison of speed between standard SPI and Quad-SPI reads.
- Clock Cycles for Read Data (`0x03`) Instruction
  - 8 Cycles for Instruction
  - 24 Cycles for Address
  - 32 Cycles for Data
- Clock Cycles for Fast Read Quad Output (`0x6B`) Instruction
  - 8 Cycles for Instruction
  - 24 Cycles for Address
  - 8 Cycles for I/O Change
  - 8 Cycles for Data

Thus, the Fast Read Quad Output (`0x6B`) Instruction requires only 48 clock cycles compared to 64 clock cycles in case of Read Data (`0x03`) Instruction making the former instruction 25% faster. The setup of SSI that achieves this is fairly similar to the one for Read Data (`0x03`) Instruction. Consider the code below,
```C
    //  - Configure SSI for Fast Read Quad Output Instruction (6Bh)
    SSI_SSIENR = 0; // Disable SSI to configure it
    SSI_CTRLR0 = (3 << 8) | (31 << 16) | (2 << 21); // Set SPI frame format to 0x2, EEPROM mode and 32 clocks per data frame
    SSI_SPI_CTRLR0 = (6 << 2) | (2 << 8) | (0x6B << 24) | (8 << 11); // Set address length to 24-bits, instruction length to 8-bits, command to Read Data (6Bh) and set wait cycles to 8
    SSI_SSIENR = 1; // Enable SSI
```
Note that compared to Read Data (`0x03`) Instruction, here the SSI is set to use Quad-SPI frame format, `XIP_CMD` is set to `0x6B` and 8 dummy clock cycles are added before data by changing `WAIT_CYCLES`. You can find the `bootStage2` code for Fast Read Quad Output (`0x6B`) Instruction in `bootStage2QuadOut.c`. Try to generate a `.uf2` file using this. You will not observe any visible difference in the blinking of LED. A more qualitative analysis of speed will be discussed in some other tutorial.

### Full Speed Ahead
Even though Fast Read Quad Output (`0x6B`) Instruction works, it is not the fastest. Fast Read Quad IO (`0xEB`) Instruction works in a slightly different way, and is significantly faster. First difference that makes this instruction faster is the ability to send the address in parallel over the four data lanes. However, the address now has to be appended with 8 Mode Bits. Thus, the 32 Bits (24 Bit address + 8 Mode Bits) can now be sent in only 8 clock cycles. This instruction also requires only 4 dummy cycles instead of 8. And, finally, if the instruction being used every time is the same, then the Mode bits can be set to `0xA0`, thus eliminating the need of sending the 8-bit instruction completely after the first message.
> [!NOTE]
> Surprisingly, this value of the Mode Bits `0xA0` and its effect is not discussed anywhere in [Flash's Datasheet](https://www.winbond.com/resource-files/w25q80dv%20dl_revh_10022015.pdf). Though, this instruction is used by Pico SDK's [second stage boot code](https://github.com/raspberrypi/pico-sdk/blob/master/src/rp2040/boot_stage2/boot2_w25q080.S).

Following diagram shows the worst case data read process using this instruction.
![img](../misc/figs/chap2/flashReadQuadIO.svg)

Note that the number of clock cycles required for the worst case is 28, which is almost 71% faster than the standard SPI, and for the best case (where the instruction is not required) is 79% faster.

> [!WARNING]
> I have not been able to make the `bootStage2QuadIO.c` to work yet. The code presented in that file and in the upcoming paragraphs is my best attempt at getting the XIP to work with Fast Read Quad IO (`0xEB`) Instruction. If you are able to figure out the mistake in my code then please let me know by opening an Issue.

To make use of this instruction, the Flash first has to be in the Quad-SPI mode, which is already discussed in the previous section. The next step is to setup SSI to perform one data read using Fast Read Quad IO (`0xEB`) Instruction. Consider the following piece of code that achieves this,
```C
    //  - Configure SSI for Fast Read Quad IO Instruction (EBh)
    SSI_SSIENR = 0; // Disable SSI to configure it
    SSI_CTRLR0 = (3 << 8) | (31 << 16) | (2 << 21); // Set SPI frame format to 0x2, EEPROM mode and 32 clocks per data frame
    SSI_SPI_CTRLR0 = (8 << 2) | (2 << 8) | (4 << 11) | (0x1 << 0); // Set address length to 32-bits (Address + Mode), instruction length to 8-bits, set wait cycles to 4, and Command in standard SPI and address in format specified by FRF
    SSI_SSIENR = 1; // Enable SSI
```
There are two differences here compare to Fast Read Quad Output (`0x6B`) Instruction case. The length of address is set to 32 Bits (24 Bits of Address + 8 Mode Bits) and SSI is set to send command in standard SPI mode and the address in Quad-SPI mode.

Now a dummy data read can be carried out with Mode Bits set to `0xA0`. The address for this task doesn't matter since this is done to let the Flash know that the same instruction (`0xEB`) will be used from the next message unless specified otherwise. Consider the following lines of code that achieves this,
```C
    //  - Perform a dummy read with mode bits set to 0xa0 to avoid sending the instruction again
    SSI_DR0 = 0xeb; // Send Fast Read Quad I/O Instruction
    SSI_DR0 = 0xa0; // Send address (0x0) + mode (0xa0) = 0b00000000000000000000000010100000
    while ((SSI_SR & (1 << 2)) && (~SSI_SR & 1)); // Wait while Transmit FIFO becomes empty and SSI is not busy
```
Note that this is achieved by writing the instruction and Addr + Mode to the Data Register (`DR0`) one after the other. An infinite `while` loop is used to make sure that the message gets transmitted before turning off SSI temporarily.

The final step is to reconfigure SSI to send no instruction and append Mode Bits to the address from now on. Following lines of code performs this configuration,
```C
    //  - Configure SSI for sending the address and mode bits only
    SSI_SSIENR = 0; // Disable SSI to configure it
    // SSI_CTRLR0 = (3 << 8) | (31 << 16) | (2 << 21); // Set SPI frame format to 0x2, EEPROM mode and 32 clocks per data frame
    SSI_SPI_CTRLR0 = (8 << 2) | (0 << 8) | (0xa0 << 24) | (4 << 11) | (0x2 << 0); // Set address length to 32-bits (Address + Mode), mode bits to append to address (0xa0), set wait cycles to 4, and Command (there is no command though) and address in format specified by FRF
    SSI_SSIENR = 1; // Enable SSI
```
Note that here the instruction length is set to zero. And, `XIP_CMD` is set to Mode Bits `0xA0` instead of the instruction, contrary to was done for other instructions. From the [RP2040 Datasheet](https://datasheets.raspberrypi.com/rp2040/rp2040-datasheet.pdf#page=606), you'd know that whether the value in `XIP_CMD` is used as a command or it is appended to the address depends on whether the instruction length is set to 8-bit or 0-bit respectively.

Unfortunately, as described in the Warning above, this code doesn't work even though it is very close to the correct working code, at least that's what I feel. In any case, hopefully this section still gave you an idea about how fast XIP, SSI and Flash can work together.