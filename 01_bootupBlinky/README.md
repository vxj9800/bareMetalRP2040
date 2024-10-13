## 1. RP2040 Boot Up Process

Let's consider the C code given below in the context of programming a Pi Pico development board.
```C
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
Even though this code may look relatively small and simple, getting it to run on RP2040 requires much more than just this code. Opposite to what was discussed for Arm<sup>&copy;</sup> &micro;C boot up process on the [main page](../README.md), RP2040 goes through a Two-Stage booting process. A very good resource on this booting process is [V. Hunter Adams's](https://vanhunteradams.com/) discussion on the [topic](https://vanhunteradams.com/Pico/Bootloader/Boot_sequence.html), which is also used to write next couple of paragraphs.

The RP2040 datasheet separates the [boot sequence](https://datasheets.raspberrypi.com/rp2040/rp2040-datasheet.pdf#page=131) into the *hardware-controlled* section, which happens before the processors begins executing the [*bootrom*](https://datasheets.raspberrypi.com/rp2040/rp2040-datasheet.pdf#page=132), and the *processor-controlled* section, during which processor cores 0 and 1 begin to execute the *bootrom*. This is the first stage of the booting process. The second stage of the booting process, which lives at the beginning of the user's program, is then executed from the *bootrom*.

The hardware-controlled section is not important for us since its job is to safely turn on the minimum number of peripherals required for the processors to begin executing code, and not actually executing the code. The processor-controlled boot sequence runs from [bootrom](https://datasheets.raspberrypi.com/rp2040/rp2040-datasheet.pdf#page=132). This sequence is *baked in* to the RP2040 chip. Following is the simplified version of the bootrom's workflow,

- If `BOOTSEL` button is not pressed then,
    - Load first 256 bytes from the Flash into the SRAM.
    - If these 256 bytes are valid then start executing it, otherwise go to the '`BOOTSEL` button is pressed' case.
- If `BOOTSEL` button is pressed then,
    - Enter USB Storage mode for program upload.

Note that three conditions need to be satisfied for running the `bootStage2` function provided above.

1. The function has to be **smaller than 256 bytes** in size, 252 bytes actually.
2. It needs to exist at the **start of the Flash**.
3. The code in bootrom should see it (the first 256 bytes of flash) as **valid**.

Let's tackle one problem at a time.

### Compiled Code - A Deeper Dive
First of all, let's compile the code provided at the [top](#1-rp2040-boot-up-process) by first copying it into [`boot2Blinky.c`](./boot2Blinky.c) file and then executing following command in the terminal
```bash
$ arm-none-eabi-gcc boot2Blinky.c -mcpu=cortex-m0plus -c -o boot2Blinky_temp.o
```
Following is the description of this command

- `arm-none-eabi-gcc` is the GCC Arm C Compiler
- `boot2Blinky.c` is the source code file that needs to be compiled.
- `-mcpu=cortex-m0plus` instructs the compiler to generate code for Cortex M0+ Arm Processor.
- `-c` asks the compiler to just compile the code but not link it, more on linking later.
- `-o boot2Blinky_temp.o` specifies the output file where the compiled code will go.

Check the size of `boot2Blinky_temp.o` file. It would be around 900 Bytes. This is much more than 256 Bytes that we are aiming for. However, a code compiled this way (it is in ELF binary format) contains much more information than just the instructions and data. We can generate the *Object Dump* of this file to extract all the information from `boot2Blinky_temp.o` file in human readable format,
```bash
$ arm-none-eabi-objdump -hSD boot2Blinky_temp.o > boot2Blinky_temp.objdump
```
If you open the `boot2Blinky_temp.objdump` in a text editor, you should see
``` {.numberLines}
boot2Blinky_temp.o:     file format elf32-littlearm

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
Note that the first major piece of information the `boot2Blinky_temp.objdump` file contains is a table listing different sections and their attributes like size, VML, LMA, etc. The important sections for us right now are

- `.text` - Contains the instructions that can be executed by the processor.
- `.data` - Contains the data or variable values.
- `.bss` - Define the data that needs to be initialized to zero.

For our case, only the `.text` section has non-zero size, which is `0x00000060`, meaning that the actual program is 96 Bytes in size.

However, note that VMA(virtual memory address)/LMA(load memory address) for all the sections are set to 0. Meaning, `boot2Blinky_temp.o` is not yet a complete firmware, because it does not contain the information where those sections should be loaded in the address map. A linker needs to be used to produce a full firmware from `boot2Blinky_temp.o`.

### The Linker Script
If you have worked on a multi file C/C++ project then you'd know that the job of a Linker is to merge multiple object files into an executable file and resolve symbol references. In case of an embedded system, the Linker performs one more essential task of defining appropriate locations for different sections of a program. To figure out which sections go where, the Linker relies on a Linker Script - a blueprint for your program. An awesome discussion on Linker Scripts is provided by [François Baldassari](https://github.com/franc0is) in his blog [From Zero to main(): Demystifying Firmware Linker Scripts](https://interrupt.memfault.com/blog/how-to-write-linker-scripts-for-firmware#from-zero-to-main-demystifying-firmware-linker-scripts). This section discusses Linker Scripts in a very concise manner.

The current goal, as discussed in [The Two Stage Booting Process](#the-two-stage-booting-process), is to put the code at the start of the Flash. Let's consider the following Linker Script for this
```ld {.numberLines}
ENTRY(bootStage2);

MEMORY
{
    flash(rx) : ORIGIN = 0x10000000, LENGTH = 2048k
    sram(rwx) : ORIGIN = 0x20000000, LENGTH = 256k
}

SECTIONS
{
    .text :
    {
        *(.text*)
    } > flash
}
```
The first line in the Linker Script tells the linker the value of the *entry point* attribute in the generated ELF header. This is an aid for a debugger that helps to set a breakpoint at the beginning of the firmware.

In order to allocate program space, the linker needs to know how much memory is available, and at what addresses that memory exists. This is what the `MEMORY` definition in the linker script is for. In the script above, the name of the memory, their access attributes, the start address and the size is provided. All this information can be found in the &micro;C's datasheet.

Finally, the placement of each section from the object files is specified in the `SECTION` definition. Only one section, `.text`, is specified in the script above since we know from our previous analysis that the `.data` and `.bss` sections are empty. Technically, lines 11 to 14 tells the linker to merge all the sections that start with `.text` name (`.text*`) from all the object files (`*(.text*)`) into a single section called `.text` in the final executable and append it (`> flash`) to the Flash.

Copy the contents of the Linker Script from above into a `link.ld` file. To produce a properly linked executable, run following command in the terminal.
```bash
$ arm-none-eabi-ld boot2Blinky_temp.o -T link.ld -nostdlib -o boot2Blinky_temp.elf
```
Here is the description of this command

- `arm-none-eabi-ld` is the GCC Arm Linker
- `boot2Blinky_temp.o` is the object file we want to link. There can be more of such files.
- `-T link.ld` specifies the Linker Script that should be used by the linker.
- `-nostdlib` restricts the linker from linking the Standard Library along with our code, more on this later.
- `-o boot2Blinky_temp.elf` specifies the output executable file path.

If you wish to compile and link in one command then you can run
```bash
$ arm-none-eabi-gcc boot2Blinky.c -mcpu=cortex-m0plus -T link.ld -nostdlib -o boot2Blinky_temp.elf
```

By generating the Object Dump of the newly created ELF file
```bash
$ arm-none-eabi-objdump -hSD boot2Blinky_temp.elf > boot2Blinky_temp.objdump
```
``` {.numberLines}
boot2Blinky_temp.elf:     file format elf32-littlearm

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
you'd notice that now there is only the `.text` section in the file and its VMA/LMA is 0x10000000, which is what we were aiming for.

A binary file can also be generated now which would represent the raw program that may run on a &micro;C. This file can be generated by executing
```bash
$ arm-none-eabi-objcopy -O binary boot2Blinky_temp.elf boot2Blinky_temp.bin
```
If you check the size of `boot2Blinky_temp.bin` file, you'd see that it is exactly 96 Bytes in size, the size of the `.text` section. Now this is something that should go into the Flash. You can also open this file using any [Binary File Viewer](https://hexed.it/) and make sure that its content match with the Disassembly of `.text` section you may find (2nd Column) in the Object Dump.

However, what happened to placing the `.text` section at the start of the Flash? That information seems to have been lost in going from ELF format to binary format. This problem is solved by the UF2 file format that is discussed in a later section.

### Making the Program Valid
As discussed previously, if the `BOOTSEL` button is not pressed then the &micro;C will load the first 256 Bytes of Flash into SRAM and start executing it after checking its validity. Since the binary generated so far is only 96 Bytes, it can easily be executed if the Pi Pico thinks it is valid.

To check the validity, the code in bootrom computes a CRC32 checksum of the first 252 Bytes and compares it with the last 4 Bytes of the 256 Bytes it has loaded from Flash to SRAM. If the computed CRC32 checksum matches with the last 4 Bytes, then the 252 Bytes are assumed to be a valid code and it starts executing from the top. Hence the goal in this section is to somehow convert the 96 Bytes binary into a 256 Bytes one and make sure that the last 4 Bytes contain a CRC32 checksum of the first 252 Bytes.

Calculation of CRC32 checksum is a big topic in itself and is not in the scope of this guide. To avoid going into too much detail, an open-source CRC calculation library called [CRCpp](https://github.com/d-bahr/CRCpp) is used here. Consider the [compCrc32.cpp](./compCrc32.cpp) file. You'll see that majority of the code here loads the contents of the binary file into a 252 Bytes long array, which is zero-initialized. After that, at line 57, the CRC32 checksum is calculated using the library discussed previously.
```C++
// Calculate CRC32 for the 252 bytes of data
unsigned char crc[4] = {0};
*reinterpret_cast<std::uint32_t*>(crc) = CRC::Calculate(binFileData, 252, CRC::CRC_32_MPEG2());
```
Note the use of `CRC_32_MPEG2()` in the code above. There exists multiple parameters in CRC32 calculation which would result in different CRC32 checksum. The parameters used by bootrom in RP2040 are discussed in [Section 2.8.1.3.1](https://datasheets.raspberrypi.com/rp2040/rp2040-datasheet.pdf#page=134) of the RP2040 datasheet. These specific set of parameters correspond to [CRC-32/MPEG-2](https://crccalc.com/?method=CRC-32/MPEG-2) algorithm.

Finally, the 4 Bytes of CRC checksum is output into a `crc.c` file. For this specific case, the resulting `crc.c` fill should contain the following,
```C {.numberLines}
__attribute__((section(".crc"))) unsigned char crc[4] = {0xc0, 0x8d, 0x02, 0x6c};
```
Note that the array definition in `crc.c` file contains `__attribute__((section(".crc")))`. This is a directive to the compiler telling it to place the `crc` array in a separate section called `.crc`. This allows us to control where this data will be placed in the Flash with the help of the Linker Script.

To generate the `crc.c`, first clone the [CRCpp](https://github.com/d-bahr/CRCpp) repo by executing following command where the `boot2Blinky_temp.bin` is located.
```bash
$ git clone https://github.com/d-bahr/CRCpp.git
```
Then, copy the code for calculating CRC32 checksum into `compCrc32.cpp` and execute following commands.
```bash
$ g++ compCrc32.cpp -o compCrc32.out
$ ./compCrc32.out boot2Blinky_temp.bin
```

Now that we have computed a valid CRC checksum for the Boot Stage 2 code, both together can be compiled into a valid binary file. But, some changes needs to be made to the `boot2Blinky.c` and the linker script before doing that.
```C
// Replace the function definition with
__attribute__((section(".boot2"))) void bootStage2(void)
```
Similar to what was done with the `crc` array, `__attribute__((section(".boot2")))` instructs the compiler to put the `bootStage2` function into a special section called `.boot2`. The reason of doing this is also to gain the flexibility of putting this function at the start of the Flash. This will become important in the upcoming tutorials where this `.boot2` section will actually contain the code that sets up the Execute-In-Place (XIP) peripheral so that the rest of the firmware can execute directly from the Flash. This way, the second stage bootloader can be differentiated from the `.text` sections of other C/C++ code.
```ld {.numberLines}
ENTRY(bootStage2);

MEMORY
{
    flash(rx) : ORIGIN = 0x10000000, LENGTH = 2048k
    sram(rwx) : ORIGIN = 0x20000000, LENGTH = 256k
}

SECTIONS
{
    .text :
    {
        *(.boot2*)
        . = ORIGIN(flash) + 252;
        *(.crc*)
    } > flash
}
```
Note that the `.text` section now contains three sub-regions. The `.boot2` section comes first, then the loading address is moved to `ORIGIN(flash) + 252 = 0x10000000 + 0xfc = 0x100000fc`, and the `.crc` section is placed there. And this complete `.text` section is appended to Flash.

A new ELF and binary file can now be generated by executing
```bash
$ arm-none-eabi-gcc boot2Blinky.c crc.c -mcpu=cortex-m0plus -T link.ld -nostdlib -o boot2Blinky.elf
$ arm-none-eabi-objcopy -O binary boot2Blinky.elf boot2Blinky.bin
```
Looking at the Object Dump of the ELF file first
```bash
$ arm-none-eabi-objdump -hSD boot2Blinky.elf > boot2Blinky.objdump
```
``` {.numberLines}
boot2Blinky.elf:     file format elf32-littlearm

Sections:
Idx Name          Size      VMA       LMA       File off  Algn
  0 .text         00000100  10000000  10000000  00010000  2**2
                  CONTENTS, ALLOC, LOAD, READONLY, CODE
  1 .comment      00000033  00000000  00000000  00010100  2**0
                  CONTENTS, READONLY
  2 .ARM.attributes 0000002c  00000000  00000000  00010133  2**0
                  CONTENTS, READONLY

Disassembly of section .text:

10000000 <bootStage2>:
10000000:	b580      	push	{r7, lr}
10000002:	b082      	sub	sp, #8

...

100000fc <crc>:
100000fc:	6c028dc0 	stcvs	13, cr8, [r2], {192}	; 0xc0
```
we see that the first section in the ELF header is now `.text` with size `0x00000100`, which is exactly 256 Bytes, and has VMA/LMA of `0x10000000` which is still the start of the Flash. Also note that the size of the `boot2Blinky.bin` file is 256 Bytes. You will also notice the CRC32 checksum defined at address `0x100000fc` in the disassembly provided later in the file.

Now we have a valid program that can be executed by the &micro;C. But wait, how will this binary be loaded into the Flash of Pi Pico? This is where one more level of complexity exists.

### The UF2 File Format
UF2 (USB Flashing Format) is designed by Microsoft to be suitable for flashing &micro;C over MSC (Mass Storage Class; aka removable flash drive). Thus, to make the programmer's life easier, the bootrom of RP2040 is designed to be able to read a UF2 file in USB Mass Storage mode, and transfer the program into the Flash attached externally to the RP2040. However, to even be able to send the program to the &micro;C, the binary file needs to be converted into a UF2 file. Fortunately, Microsoft's [uf2](https://github.com/microsoft/uf2) contains utilities that allow us to do this pretty quickly.

To convert the `boot2Blinky.bin` file into `boot2Blinky.uf2`, first clone the [uf2](https://github.com/microsoft/uf2) repo into the directory that contains the `boot2Blinky.bin` file.
```bash
$ git clone https://github.com/microsoft/uf2.git
```
Now execute
```bash
$ python3 ./uf2/utils/uf2conv.py -b 0x10000000 -f 0xe48bff56 -c boot2Blinky.bin -o boot2Blinky.uf2
```
Following is the description of this command
- `./uf2/utils/uf2conv.py` is the Python script that can convert a binary into a UF2 file.
- `-b 0x10000000` specifies the base address where the binary should be copied at after the UF2 file is read by the &micro;C.
- `-f 0xe48bff56` is the [familyID](https://github.com/microsoft/uf2/blob/master/utils/uf2families.json) of the RP2040 &micro;C.
- `-c boot2Blinky.bin` specifies the binary file that needs to be converted.
- `-o boot2Blinky.uf2` specifies the output UF2 file.

Done, finally! :relieved:

Now you have a `boot2Blinky.uf2` that you can upload to a Pi Pico and see the fruits of your labor.

### Clean Up the Mess
Going through everything discussed so far, you must have generated many files and have typed a good amount of commands in the terminal. The GNU Make utility can be used to keep the folder containing the code clean and automate the build process.

GNU Make, `make`, utility uses a configuration file named `Makefile` where it reads instructions of a recipe. This automation is great because it also documents the process of building firmware, used compilation flags, etc.

There is a great Makefile tutorial at [https://makefiletutorial.com](https://makefiletutorial.com) - for those new to `make`. Below, the most essential concepts required to understand a simple bare metal `Makefile` are discussed. Those who already familiar with `make`, can skip this section.

The `Makefile` format is simple:
```make
recipe1: rawMaterial1 rawMaterial2
	instruction ...             # Comments can go after hash symbol
	instruction ....            # IMPORTANT: instruction must be preceded with the TAB character

recipe2: recipe1 rawMaterial3   # A recipe can be a raw material for another recipe
	instruction ...             # Don't forget about TAB. Spaces won't work!
```
Now, `make` with a recipe name (also called target) can be executed:
```bash
$ make recipe1
```
It is possible to define variables and use them in instructions. Also, recipes can be file names that needs to be created:
```make
# Source code files
BOOT2 = boot2Blinky

# Directory to create temporary build files in
BUILDDIR = build

# Compilation related variables
TOOLCHAIN = arm-none-eabi-
CFLAGS ?= -mcpu=cortex-m0plus

makeDir:
	mkdir -p $(BUILDDIR)

$(BUILDDIR)/$(BOOT2).bin:
    $(TOOLCHAIN)gcc $(CFLAGS) $(BOOT2).c -c -o $(BUILDDIR)/$(BOOT2)_temp.o
```
And, any recipe can have a list of raw materials. For example, `boot2Blinky.bin` depends on our source file `boot2Blinky.c`. Whenever `boot2Blinky.c` file changes, the `make build` command rebuilds `boot2Blinky.bin`:
```make
$(BUILDDIR)/$(BOOT2).bin: $(BOOT2).c
	$(TOOLCHAIN)gcc $(CFLAGS) $(BOOT2).c -c -o $(BUILDDIR)/$(BOOT2)_temp.o
    ...
```
Now take a look at the `Makefile` provided in this folder. Try to replace `$(VARIABLE)` with the value of the variable to see what commands are being executed. The variable `$@` expands to the name of the recipe being built. If you execute `make build` in the terminal, you should see the following output,
```bash
$ make build
mkdir -p build
arm-none-eabi-gcc -mcpu=cortex-m0plus boot2Blinky.c -c -o build/boot2Blinky_temp.o
arm-none-eabi-objdump -hSD build/boot2Blinky_temp.o > build/boot2Blinky_temp.objdump
arm-none-eabi-objcopy -O binary build/boot2Blinky_temp.o build/boot2Blinky_temp.bin
g++ -I ../utils compCrc32.cpp -o build/compCrc32.out
./build/compCrc32.out build/boot2Blinky_temp.bin
arm-none-eabi-gcc boot2Blinky.c build/crc.c -mcpu=cortex-m0plus -T link.ld -nostdlib -o build/boot2Blinky.elf
arm-none-eabi-objdump -hSD build/boot2Blinky.elf > build/boot2Blinky.objdump
arm-none-eabi-objcopy -O binary build/boot2Blinky.elf build/boot2Blinky.bin
python3 ../utils/uf2/utils/uf2conv.py -b 0x10000000 -f 0xe48bff56 -c build/boot2Blinky.bin -o build/boot2Blinky.uf2
Converted to uf2, output size: 512, start address: 0x10000000
Wrote 512 bytes to build/boot2Blinky.uf2
cp build/boot2Blinky.uf2 ./boot2Blinky.uf2
```
There are three things to note here.
1. The supporting libraries, `uf2` and `CRCpp`, are moved to a separate folder called `utils`. The `Makefile` provided here is designed to account for this.
2. The `Makefile` is designed to produce all the new files into a `build` directory and copy only the `boot2Blinky.uf2` to the current directory. This keeps the current folder clean and manageable.
3. An extra recipe called `clean` is provided which is not a raw material for the `build` recipe. Its job is to delete the `build` directory and the `boot2Blinky.uf2`.

### Respect the Constraints
&micro;Cs are quite limited in terms of resources whether it be clock speed, RAM size or availability of Flash. Thus, it is always desirable that the code being written is optimized to reduce the size of the final binary, improve the RAM usage and conserve processor clock cycles. Fortunately, you don't have to do too much to achieve this, the compiler and the linker will do this for you. Do following changes in the `Makefile` provided here to optimize your code,
```make
CFLAGS ?= -mcpu=cortex-m0plus -O3
LDFLAGS ?= -T link.ld -nostdlib -O3
```
Now take a look at the `boot2Blinky_temp.objdump`. You'd see that the size of the `.boot2` section has shrunk down to `0x38` from `0x60`. This is 41% reduction in code size. That's huge! However, if you try to execute this code on the &micro;C, it'll not run (or at least it won't run as expected). This is because the optimizations are being applied to the register accesses as well. A register's value can change without the code/software explicitly changing it. This information needs to be provided to the compiler. Because of this, all the register macros need to contain `volatile` keyword as shown below,
```C
#define RESETS_RESET *(volatile uint32_t *) (0x4000c000)
```
Make such change to all the register macros and compile the code again. This time you'd see that the size of `.boot2` section has increased to `0x44`, which is 29% smaller than the original size. This is still significant improvement in code size.

If you run the code now, you should the LED blinking again. But wait, something has changed, the LED blinks faster now. This is because the number of instructions used or the type of instructions used makes the execution of one `while` loop complete much faster than before. In other words, less clock cycles are used to perform the same operations, hence the code runs faster. The qualitative analysis of the computational improvement is not possible at this stage, so this will be discussed in some other tutorial.

This folder is now ready to serve as a starting point for creating a working Second Stage Boot-loader.