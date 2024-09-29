## 2. Execute Code from Flash 
In the last chapter, you learned the boot-up process of RP2040 &micro;C. In summary, the process is relatively simple, the bootrom verifies that the first 256 Bytes in Flash is valid, loads it into RAM and start executing it. But, the RP2040 can work with any Flash chip that can communicate over SPI protocol and is smaller than 16 MBytes in size. The development board used in these tutorials (Pi Pico) has 2 MBytes of Flash. It also means that it should be able to execute a program that is larger than 256 Bytes. To achieve this, RP2040 contains XIP (Execute-In-Place) peripheral. The idea is simple, convert the read accesses requests from the processor into SPI commands. Thus, XIP acts a transparent Bus-To-SPI translation layer between the RP2040's processor and the Flash that can communicate over SPI. The goal of this chapter is to setup the XIP so that the code can be executed directly from Flash. In fact, the 256 Bytes code that is loaded to RAM by bootrom and executed is supposed to configure XIP, this is by design.

### SPI Protocol
Let's first brush-up on Serial Peripheral Interface (SPI) before discussing its variants Double-SPI and Quad-SPI.

SPI was originally developed by Motorola in 1980s. It is used heavily in today's time for communication between two &micro;Cs, a &micro;C and sensor/s or actuator/s. There are may flavors of SPI as well, like Microwire (developed by National Semiconductor) and Synchronous Serial Protocol (developed by Texas Instruments). Motorola's version of SPI is discussed here.

![image](../misc/figs/chap2/connectPicoSPI.svg)



Refs:
- [rp2040 datasheet pg 572](https://datasheets.raspberrypi.com/rp2040/rp2040-datasheet.pdf#page=573)
- [W25Q80DV datasheet](https://www.winbond.com/resource-files/w25q80dv%20dl_revh_10022015.pdf)
- [rp2040 boot stage 2](https://github.com/raspberrypi/pico-sdk/blob/master/src/rp2040/boot_stage2/boot2_w25q080.S)
- [QSPI Basics](https://embeddedinventor.com/quad-spi-everything-you-need-to-know/)
- [STMicro QSPI Discussion](https://www.st.com/content/ccc/resource/technical/document/application_note/group0/b0/7e/46/a8/5e/c1/48/01/DM00227538/files/DM00227538.pdf/jcr:content/translations/en.DM00227538.pdf)
- [ARMv6-M exception model](https://cdn.hackaday.io/files/1770827576276288/DDI0419E_armv6m_arm.pdf#page=191)
- [Inline assembly use c variables](https://stackoverflow.com/questions/14628885/manipulating-c-variable-via-inline-assembly)

The aim of this chapter is to introduce spi, xip, flash, jump to a function, interrupt handlers and vector table.