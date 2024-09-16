## 1. RP2040 Boot Up Process

Let's consider the following code C code in the context of programming a Pi Pico development board.
```C {.numberLines}
#include <stdint.h>
#include <stdbool.h>

// Define necessary register addresses
#define RESETS_RESET *(uint32_t *) (0x4000c000)
#define IO_BANK0_GPIO25_CTRL *(uint32_t *) (0x400140cc)
#define SIO_GPIO_OE_SET *(uint32_t *) (0xd0000024)
#define SIO_GPIO_OUT_XOR *(uint32_t *) (0xd000001c)

// Main entry point
void bootStage2(void)
{
    // Bring IO_BANK0 out of reset state
    RESETS_RESET &= ~(1 << 5);

    // Set GPIO 25 function to SIO
    IO_BANK0_GPIO25_CTRL = 5;

    // Set output enable for GPIO 25 in SIO
    SIO_GPIO_OE_SET |= 1 << 25;

    while (true)
    {
        // Wait for some time
        for (uint32_t i = 0; i < 100000; ++i);

        // Flip output for GPIO 25
        SIO_GPIO_OUT_XOR |= 1 << 25;
    }
}
```
This code is trying to blink the LED attached with GPIO25 on Pi Pico. Your first assignment is to read up on the sections from the [RP2040 datasheet](https://datasheets.raspberrypi.com/rp2040/rp2040-datasheet.pdf) in which the registers used in this code are defined.

### The Two Stage Booting Process
Even though this code may look relatively small and simple, getting it to run on RP2040 requires much more than just this code. Opposite to what was discussed for Arm^:copyright:^ &micro;C boot up process on the [main page](../README.md), RP2040 goes through a Two-Stage booting process. A very good resource on this booting process is [V. Hunter Adams's](https://vanhunteradams.com/) discussion on the [topic](https://vanhunteradams.com/Pico/Bootloader/Boot_sequence.html), which is also used to write next couple of paragraphs.

The RP2040 datasheet separates the [boot sequence](https://datasheets.raspberrypi.com/rp2040/rp2040-datasheet.pdf#page=131) into the *hardware-controlled* section which happens before the processors begin executing the [*bootrom*](https://datasheets.raspberrypi.com/rp2040/rp2040-datasheet.pdf#page=132), and the *processor-controlled* section during which processor cores 0 and 1 begin to execute the *bootrom*. This is the first stage of the booting process and the second stage of the booting process (which lives at the beginning of the user's program) then runs after the *bootrom*.

The hardware-controlled section is not important for us since its job is to safely turn on the minimum number of peripherals required for the processors to begin executing code, and not actually executing the code. The processor-controlled boot sequence runs from ROM. This sequence is *baked in* to the RP2040 chip. Following is the simplified version of the bootrom's workflow,

- If `BOOTSEL` button is not pressed then,
    - Load first 256 bytes from the Flash into the SRAM.
    - If these 256 bytes are valid then start executing it, otherwise go to the '`BOOTSEL` button is pressed' case.
- If `BOOTSEL` button is pressed then,
    - Enter USB Storage mode for program upload.

Note that three conditions need to be satisfied for running the `bootStage2` function provided above.

1. The function has to be **smaller than 256 bytes** in size, 252 bytes actually.
2. It needs to exist at the **start of the Flash**.
3. The code in bootrom should detect it (the first 256 bytes of flash) as **valid**.

Let's tackle one problem at a time.

### Compiled Code - A Deeper Dive
First of all, go ahead and copy the code from the [top](#1-rp2040-boot-up-process) into a `boot2Blinky.c` file and compile it by executing
```bash
$ arm-none-eabi-gcc -mcpu=cortex-m0plus -mthumb boot2Blinky.c -c -o boot2Blinky.o
```
Following is the description of this command

- `arm-none-eabi-gcc` is the GCC Arm C Compiler
- `-mcpu=cortex-m0plus` instructs the compiler to generate code for Cortex M0+ Arm Processor.
- `-mthumb` tells the compiler to use Thumb2 instruction set instead of Arm instruction set. More on this later.
- `boot2Blinky.c` is the source code file that needs to be compiled.
- `-c` asks the compiler to just compile the code but not link it, more on linking later.
- `-o boot2Blinky.o` specifies the output file where the compiled code will go.

Check the size of `boot2Blinky.o` file. It would be around 900 Bytes. This is much more than 256 Bytes that we are aiming for. However, a code compiled this way (it is in ELF binary format) contains much more information than just the instructions and data. We can generate the *Object Dump* of this file to extract all the information available in the `boot2Blinky.o` file,
```bash
$ arm-none-eabi-objdump -hSD boot2Blinky.o > boot2Blinky.objdump
```
If you open the `boot2Blinky.objdump` in a text editor, you should see
``` {.numberLines}
boot2Blinky.o:     file format elf32-littlearm

Sections:
Idx Name          Size      VMA       LMA       File off  Algn
  0 .text         00000060  00000000  00000000  00000034  2**2
                  CONTENTS, ALLOC, LOAD, READONLY, CODE
  1 .data         00000000  00000000  00000000  00000094  2**0
                  CONTENTS, ALLOC, LOAD, DATA
  2 .bss          00000000  00000000  00000000  00000094  2**0
                  ALLOC
  3 .comment      00000034  00000000  00000000  00000094  2**0
                  CONTENTS, READONLY
  4 .ARM.attributes 0000002c  00000000  00000000  000000c8  2**0
                  CONTENTS, READONLY

Disassembly of section .text:

00000000 <bootStage2>:
   0:	b580      	push	{r7, lr}
   2:	b082      	sub	sp, #8
   ...
```
Note that the first major piece of information the `boot2Blinky.objdump` file contains is a table listing different sections and their attributes like size, VML, LMA, etc. The important sections for us right now are

- `.text` - Contains the instructions that can be executed by the processor.
- `.data` - Contains the data or variable values.
- `.bss` - Define the data that needs to be initialized to zero.

For our case, only the `.text`section has non-zero size, which is `0x00000060` meaning that the actual program is 96 Bytes in size.

However, note that VMA(virtual memory address)/LMA(load memory address) for sections are set to 0 - meaning, `boot2Blinky.o` is not yet a complete firmware, because it does not contain the information where those sections should be loaded in the address map. We need to use a linker to produce a full firmware from `boot2Blinky.o`.

### The Linker Script
If you have worked on a multi file C/C++ project then you'd know that the job of a Linker is to merge multiple object files into an executable file and resolve symbol references. In case of an embedded system, the Linker performs one more essential task of defining appropriate locations for different sections of a program. To figure out which bits go where, the Linker relies on a Linker Script - a blueprint for your program. An awesome discussion on Linker Scripts is provided by [François Baldassari](https://github.com/franc0is) in his blog [From Zero to main(): Demystifying Firmware Linker Scripts](https://interrupt.memfault.com/blog/how-to-write-linker-scripts-for-firmware#from-zero-to-main-demystifying-firmware-linker-scripts). This section discusses Linker Scripts in a very concise manner.

The current goal, as discussed in [The Two Stage Booting Process](#the-two-stage-booting-process), is to put the code at the start of the Flash. Let's consider the following Linker Script for this
``` {.numberLines}
ENTRY(bootStage2);

MEMORY
{
    flash(rx) : ORIGIN = 0x10000000, LENGTH = 2048k
    sram(rwx) : ORIGIN = 0x20000000, LENGTH = 256k
}

SECTIONS
{
    .text  : { *(.text*) }       > flash
}
```
The first line in the Linker Script tells the linker the value of the *entry point* attribute in the generated ELF header. This is an aid for a debugger that helps to set a breakpoint at the beginning of the firmware.

In order to allocate program space, the linker needs to know how much memory is available, and at what addresses that memory exists. This is what the `MEMORY` definition in the linker script is for. In the script above, the name of the memory, their access attributes, the start address and the size is provided. All this information can be found in the &micro;C's datasheet.

Finally, the placement of each section from the object files is specified in the `SECTION` definition. Only one section, `.text`, is specified in the script above since we know from our previous analysis that the `.data` and `.bss` sections are empty. Technically, line 11 tells the linker to merge all the sections that start with `.text` name (`.text*`) from all the object files (`*(.text*)`) should be merged into a single section called `.text` in the final executable and should be appended to the Flash. This is exactly what we are trying to achieve.

Copy the contents of the Linker Script from above into a `link.ld` file. To compile the source code and link it, execute following in the terminal.
```bash
$ arm-none-eabi-ld boot2Blinky.o -T link.ld -nostdlib -o boot2Blinky.elf
```
Following is the description of this command

- `arm-none-eabi-ld` is the GCC Arm Linker
- `boot2Blinky.o` is the object file we want to link. There can be more of such files.
- `-T link.ld` specifies the Linker Script that should be used by the linker.
- `-nostdlib` restricts the linker from linking the Standard Library along with our code, more on this later.
- `-o boot2Blinky.elf` specifies the output executable file path.

If you wish to compile and link in one command then you can execute
```bash
$ arm-none-eabi-gcc -mcpu=cortex-m0plus -mthumb boot2Blinky.c -T link.ld -nostdlib -o boot2Blinky.elf
```

By generating the Object Dump of the newly created ELF file
```bash
$ arm-none-eabi-objdump -hSD boot2Blinky.elf > boot2Blinky.objdump
```
``` {.numberLines}
boot2Blinky.elf:     file format elf32-littlearm

Sections:
Idx Name          Size      VMA       LMA       File off  Algn
  0 .text         00000060  10000000  10000000  00010000  2**2
                  CONTENTS, ALLOC, LOAD, READONLY, CODE
  1 .comment      00000033  00000000  00000000  00010060  2**0
                  CONTENTS, READONLY
  2 .ARM.attributes 0000002c  00000000  00000000  00010093  2**0
                  CONTENTS, READONLY

Disassembly of section .text:

10000000 <bootStage2>:
10000000:       b580            push    {r7, lr}
10000002:       b082            sub     sp, #8
```
you'd notice that now there is only the `.text` section in the file and its VMA/LMA is 0x10000000, which we were aiming for.

A binary file can also be generated now which would represent the raw program that may run on a &micro;C. This file can be generated by executing
```bash
$ arm-none-eabi-objcopy -O binary boot2Blinky.elf boot2Blinky.bin
```
If you check the size of `boot2Blinky.bin` file, you'd see that it is exactly 96 Bytes in size, the size of the `.text` section. Now this is something that should go into the Flash. You can also open this file using any [Binary File Viewer](https://hexed.it/) and make sure that its content match with the Disassembly of `.text` section you may find (2nd Column) in the Object Dump.

However, what happened to placing the `.text` section at the start of the Flash? That information seems to have been lost in going from ELF format to binary format. This problem is solved by the UF2 file format that is discussed in the next section.

### Making the Program Valid
As discussed previously, if the `BOOTSEL` button is not pressed then the &micro;C will load the first 256 Bytes of Flash into SRAM and start executing it after checking its validity. Since the binary generated so far is only 96 Bytes, it can easily be executed if the Pi Pico thinks it is valid.

To check the validity, the code in bootrom computes a CRC32 checksum of the first 252 Bytes and compares it with the last 4 Bytes of the 256 Bytes it has loaded from Flash to SRAM. If the computed CRC32 checksum matches with the last 4 Bytes, then the 252 Bytes are assumed to be a valid code and it starts executing from the top. Hence the goal in this section is to somehow convert the 96 Bytes binary into a 256 Bytes one and make sure that the last 4 Bytes contain a CRC32 checksum of the first 252 Bytes.

Calculation of CRC32 checksum is a big topic in itself and is not in the scope of this guide. To avoid going into too much detail, an open-source CRC calculation library called [CRCpp](https://github.com/d-bahr/CRCpp) is used here. Consider the following C++ program.
```C++ {.numberLines}
#include <fstream>
#include <iostream>
#include <filesystem>

#include "CRCpp/inc/CRC.h"

int main(int argc, char *argv[])
{
    std::filesystem::path binFilePath;
    std::ifstream binFile;

    // Make sure that an argument is entered
    if (argc > 1)
    {
        binFilePath = argv[1];

        // Bail if file name doesn't have .bin extension
        if (binFilePath.extension() != ".bin")
        {
            std::cout << "The input file must have .bin extension. Exiting ..." << std::endl;
            return 1;
        }

        // Bail if the file doesn't exist
        if (!std::filesystem::exists(binFilePath))
        {
            std::cout << "Could not locate file: " << binFilePath << ". Exiting ..." << std::endl;
            return 1;
        }

        // Open the file
        binFile.open(binFilePath, std::ios::binary);
        binFile.seekg(0, std::ios::end);
        size_t binFileSize = binFile.tellg();
        binFile.seekg(0, std::ios::beg);

        // Bail if it is > 252 bytes in size
        if (binFileSize > 252)
        {
            std::cout << "The input must be 252 Bytes in size at max. Exiting ..." << std::endl;
            return 1;
        }

        // All good, now load the file into an array
        unsigned char binFileData[256] = {0};
        binFile.read((char *)binFileData, 252);
        binFile.close();

        // Calculate CRC32 for the first 252 bytes of data
        unsigned char crc[4] = {0};
        *reinterpret_cast<std::uint32_t*>(crc) = CRC::Calculate(binFileData, 252, CRC::CRC_32_MPEG2());

        // Place the crc value at the end of the array
        for (size_t i = 0; i < 4; ++i)
            binFileData[252 + i] = crc[i];

        // Output the contents of the array to a .cpp file
        std::ofstream cppFile(binFilePath.replace_filename("boot2").replace_extension("c"));
        cppFile << "unsigned char boot2[256] __attribute__((section(\".boot2\"))) = {" << std::endl;
        for (size_t i = 0; i < 256; ++i)
            cppFile << "0x" << std::setw(2) << std::setfill('0') << std::hex << (unsigned int)binFileData[i] << ((i + 1) % 16 == 0 ? ",\n" : ",");
        cppFile << "};";
        cppFile.close();
    }
    else
    {
        std::cout << "An input file with .bin extension must be provided. Exiting ..." << std::endl;
        return 1;
    }

    return 0;
}
```

You'll see that majority of the code here loads the contents of the binary file into a 256 Bytes long array, which is zero-initialized. After that, at line 51, the CRC32 checksum is calculated using the library discussed previously.
```C++
// Calculate CRC32 for the first 252 bytes of data
unsigned char crc[4] = {0};
*reinterpret_cast<std::uint32_t*>(crc) = CRC::Calculate(binFileData, 252, CRC::CRC_32_MPEG2());

// Place the crc value at the end of the array
for (size_t i = 0; i < 4; ++i)
    binFileData[252 + i] = crc[i];
```
Note the use of `CRC_32_MPEG2()` in the code above. There exists multiple parameters in CRC32 calculation which would result in different CRC32 checksum. The parameters used by bootrom in RP2040 are discussed in [Section 2.8.1.3.1](https://datasheets.raspberrypi.com/rp2040/rp2040-datasheet.pdf#page=134) of the RP2040 datasheet. These specific set of parameters correspond to [CRC-32/MPEG-2](https://crccalc.com/?method=CRC-32/MPEG-2) algorithm. The computed CRC32 value is placed at the end of 256 Bytes array here.

Finally, the full contents of the 256 Bytes array is output into a `boot2.c` file. For this specific case, the resulting `boot2.c` fill will contain the following,
```C {.numberLines}
unsigned char boot2[256] __attribute__((section(".boot2"))) = {
0x80,0xb5,0x82,0xb0,0x00,0xaf,0x11,0x4b,0x1a,0x68,0x10,0x4b,0x20,0x21,0x8a,0x43,
0x1a,0x60,0x0f,0x4b,0x05,0x22,0x1a,0x60,0x0e,0x4b,0x1a,0x68,0x0d,0x4b,0x80,0x21,
0x89,0x04,0x0a,0x43,0x1a,0x60,0x00,0x23,0x7b,0x60,0x02,0xe0,0x7b,0x68,0x01,0x33,
0x7b,0x60,0x7b,0x68,0x08,0x4a,0x93,0x42,0xf8,0xd9,0x08,0x4b,0x1a,0x68,0x07,0x4b,
0x80,0x21,0x89,0x04,0x0a,0x43,0x1a,0x60,0xed,0xe7,0xc0,0x46,0x00,0xc0,0x00,0x40,
0xcc,0x40,0x01,0x40,0x24,0x00,0x00,0xd0,0x9f,0x86,0x01,0x00,0x1c,0x00,0x00,0xd0,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xc0,0x8d,0x02,0x6c,
};
```
Note that the array definition in `boot2.c` file contains `__attribute__((section(".boot2")))`. This is a directive to the compiler telling it to place the `boot2` array in a separate section called `.boot2`. This will become important in the upcoming tutorials where this `.boot2` section will actually contain the code that sets up the Execute-In-Place (XIP) peripheral so that the rest of the firmware can execute directly from the Flash.

To generate the `boot2.c`, first clone the [CRCpp](https://github.com/d-bahr/CRCpp) repo by executing following command where the `boot2Blinky.bin` is located.
```bash
$ git clone https://github.com/d-bahr/CRCpp.git
```
Then, copy the code for padding CRC32 checksum into `padCrc32.cpp` and execute following commands.
```bash
$ g++ padCrc32.cpp -o padCrc32.out
$ ./padCrc32.out boot2Blinky.bin
```

Now that we a valid Boot Stage 2 code, it can be compiled into a valid binary file. But, following changes needs to be made to the linker script before doing that
```
ENTRY(boot2); /* Changed */

MEMORY
{
    flash(rx) : ORIGIN = 0x10000000, LENGTH = 2048k
    sram(rwx) : ORIGIN = 0x20000000, LENGTH = 256k
}

SECTIONS
{
    .boot2 : { *(.boot2*) }      > flash  /* Changed */
}
```
Note that the entry point is now changed to `boot2` and the `SECTIONS` contain only the `.boot2` section since that is the section containing the code this time around.

A new ELF and binary files can now be generated by executing
```bash
$ arm-none-eabi-gcc -mcpu=cortex-m0plus -mthumb boot2.c -T link.ld -nostdlib -o boot2.elf
$ arm-none-eabi-objcopy -O binary boot2Blinky.elf boot2Blinky.bin
```
Looking at the Object Dump of the ELF file first
```bash
$ arm-none-eabi-objdump -hSD boot2.elf > boot2.objdump
```
``` {.numberLines}
boot2.elf:     file format elf32-littlearm

Sections:
Idx Name          Size      VMA       LMA       File off  Algn
  0 .boot2        00000100  10000000  10000000  00010000  2**2
                  CONTENTS, ALLOC, LOAD, DATA
  1 .comment      00000033  00000000  00000000  00010100  2**0
                  CONTENTS, READONLY
  2 .ARM.attributes 00000032  00000000  00000000  00010133  2**0
                  CONTENTS, READONLY

Disassembly of section .boot2:

10000000 <boot2>:
10000000:       b082b580        addlt   fp, r2, r0, lsl #11
10000004:       4b11af00        blmi    1046bc0c <boot2+0x46bc0c>
```
we see that the first section in the ELF header is now `.boot2` and its size is `0x00000100`, which is exactly 256 Bytes, and has VMA/LMA of `0x10000000` which is still the start of the Flash. Also note that the size of the `boot2.bin` file is 256 Bytes.

You may also notice that the Disassembly of the `.boot2` section looks slightly different than what you had for `boot2Blinky.objdump`. However, this is not actually true. The code is exactly same, compare the first two instructions of the `.text` section of `boot2Blinky.objdump` and the first instruction of the `.boot2` section of `boot2.objdump`. The problem here is that the code is stored as an array, so the disassembler is treating it as data and not code. But, it will still execute fine.

Now we have a valid program that can be executed by the &micro;C. But wait, how will this binary be loaded into the Flash of Pi Pico? This is where one more level of complexity exists.

### The UF2 File Format
UF2 (USB Flashing Format) is designed by Microsoft to be suitable for flashing &micro;C over MSC (Mass Storage Class; aka removable flash drive). Thus, to make the programmer's life easier, the bootrom of RP2040 is designed to be able to read a UF2 file in USB Mass Storage mode, and transfer the program into the Flash attached externally to the RP2040. However, to even be able to send the program to the &micro;C, the binary file needs to be converted into a UF2 file. Fortunately, Microsoft's [uf2](https://github.com/microsoft/uf2) contains utilities that allow us to do this pretty quickly.

To convert the `boot2.bin` file into `boot2.uf2`, first clone the [uf2](https://github.com/microsoft/uf2) repo into the directory that contains the `boot2.bin` file.
```bash
$ git clone https://github.com/microsoft/uf2.git
```
Now execute
```bash
python3 ./uf2/utils/uf2conv.py -b 0x10000000 -f 0xe48bff56 -c boot2.bin -o boot2.uf2
```
Following is the description of this command
- `./uf2/utils/uf2conv.py` is the Python script that can convert a binary into a UF2 file.
- `-b 0x10000000` specifies the base address where the binary should be copied at after the UF2 file is read by the &micro;C.
- `-f 0xe48bff56` is the [familyID](https://github.com/microsoft/uf2/blob/master/utils/uf2families.json) of the RP2040 &micro;C.
- `-c boot2.bin` specifies the binary file that needs to be converted.
- `-o boot2.uf2` specifies the output UF2 file.

Done, finally! :relieved:

Now you have a `boot2.uf2` that you can upload to a Pi Pico and see the fruits of your labor.