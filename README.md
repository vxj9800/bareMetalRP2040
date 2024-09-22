# Bare Metal RP2040: A Step-By-Step Guide
This project started with a pre-conceived notion that Pico C/C++ SDK contains a lot of bloat (although I am pretty sure this is not at all true :sweat_smile:). However, soon the goal of this project became understanding how any Arm<sup>&copy;</sup> microcontroller (&micro;C) can be programmed using a generic compiler and the datasheet of the &micro;C, i.e. without using any manufacturer provided framework/SDK.

A good amount of discussion in this guide is inspired from the [bare-metal-programming-guide](https://github.com/cpq/bare-metal-programming-guide) repository by [Sergey Lyubka](https://github.com/cpq). However, the discussion there is prepared for NUCLEO-F429ZI development board which houses an STM32F429ZI &micro;C. The RP2040 &micro;C on the other hand, which this guide focusses on, is quite different compared to STM32F429ZI in terms of boot-up process, and thus requires many extra steps before any code can be executed on it.

This guide is designed to be followed chapter-after-chapter, each chapter tackling one challenge of bare metal programming at a time. The chapters are designed in such a way that they build up on previous chapter's knowledge, not code content. Although can be used as the starting point for the next, each chapter provided here contains all the necessary source code, linker script (explained later) and Makefile to build a stand-alone binary that can be executed on the &micro;C.

Even though this guide is prepared in "For Dummies" format, not everything required for programming a &micro;C can be taught here. So, the readers are expected to know the following,
1. Basic C programming (Knowledge of pointers is mandatory)
2. How to compile and execute C code using a Linux terminal
3. Knowledge of basic communication protocols on a &micro;C like, UART, I2C, SPI, etc.
4. Basic Unix/Linux terminal commands like, `mkdir`, `rm`, `git`, etc.
5. How to read a datasheet (The answer is with focus and patience).

## Why this guide?
The first question someone may ask after starting to read this guide is why, why is this guide required. Isn't the [bare-metal-programming-guide](https://github.com/cpq/bare-metal-programming-guide) enough, since this guide is based on it. The answer, even though partially given previously, is more of yes and no. When this guide was initially being prepared, the aim was to only discuss the boot-up process of RP2040 and that's it. The reason for discussing this is provided in the second paragraph at the very [top](#bare-metal-rp2040-a-step-by-step-guide). However, while preparing that discussion, many other bare metal programming related challenges were discovered that are not discussed in [bare-metal-programming-guide](https://github.com/cpq/bare-metal-programming-guide). Hence, this guide.

## Prerequisites
Following tools will be required to proceed further,
1. A development board that houses an RP2040 &micro;C
2. A Text Editor application, [VSCode](https://code.visualstudio.com/), [Notepad++](https://notepad-plus-plus.org/), etc.
3. Arm Toolchain
4. GNU Make
5. Git

You can quickly install the toolchain, Make and Git by executing following commands in the terminal.
```bash
$ sudo apt update
$ sudo apt install make gcc-arm-none-eabi libnewlib-arm-none-eabi build-essential g++ libstdc++-arm-none-eabi-newlib
```

## Let's Get Started
There are some questions that should be answered before moving forward with the Bare-Metal programming.

### How does a Processor work?
In simple words, a processors job is to fetch stored program, interpret it and execute the associated operation.

The program is commonly stored in a Non-Volatile memory. Non-Volatile memory is a type of memory that doesn't lose information between power-cycles. Common modern day examples in the computer world are Solid State Drive (SSD), Hard Disk Drive (HDD), Compact Disk (CD), etc. In &micro;C world, the Non-Volatile memory is called [Flash](https://en.wikipedia.org/wiki/Flash_memory).

A program is made up of two things,
1. Instructions: Operations that the processor is supposed to perform
2. Data: Operands that the operations are performed upon.

The instructions are constantly fetched directly from Flash. However, the data, on which the processor is operating, lives in the RAM (Random Access Memory). Flash is perfect for always sending information (instructions) to the processor in a linear fashion. On the other hand, RAM provides high-speed read and write access to any information present in it, thus being suitable for storing the data.

There is one more layer of information storage in a processor, known as Registers. These containers are an inherent part of the processor. They not only store the information, but can also affect the hardware, e.g. activating 3.3V output on a pin of the processor. Thus, registers allow activation or deactivation of different hardware aspects sitting around the processor.

### The Address Map
In a &micro;C, all the components discussed in the previous section (and more known as peripherals) are fit into a single chip (for most cases). And, from programming perspective as well, these components live at fixed addresses (pointers) so that they can be accessed easily. A list of such addresses for a &micro;C is known as an Address Map. The summary of full Address Map of RP2040 is given blow ([RP2040 Datasheet, p.g. 24](https://datasheets.raspberrypi.com/rp2040/rp2040-datasheet.pdf#page=25))

| Region                        | Address      |
| ----------------------------- | ------------ |
| ROM                           | `0x00000000` |
| XIP                           | `0x10000000` |
| SRAM                          | `0x20000000` |
| APB Peripherals               | `0x40000000` |
| AHB-Lite Peripherals          | `0x50000000` |
| IOPORT Registers              | `0xd0000000` |
| Cortex-M0+ internal registers | `0xe0000000` |

Note that the Address Map is divided into different regions like ROM, SRAM, IOPORT Registers, etc. You can see that the RAM for RP2040 starts at address `0x20000000` and ends at `0x2fffffff`. Does this mean RP2040 has 1GB of RAM? No. This is just an address range that is allocated for accessing the RAM, but it doesn't mean that the RAM is actually that big. You'll have to start digging deeper into [SRAM](https://datasheets.raspberrypi.com/rp2040/rp2040-datasheet.pdf#page=123) section of the datasheet to know that the RAM in RP2040 is 264kB in size.

### Register Access
Learning to access (read or write) data at different addresses is one of the most important things you'll have to learn to start programming a &micro;C. Let's take an example. Let's say you have an LED connected between GPIO25 and GND pin of the RP2040, this is also how the LED on Pi Pico is mounted on the PCB. You can make the LED light up by making GPIO25 produce 3.3V. You will have to figure out which registers need to be changed to achieve this. You can start reading [Section 2.19](https://datasheets.raspberrypi.com/rp2040/rp2040-datasheet.pdf#page=237) of the RP2040 datasheet for this. After setting up most of the registers, what you are left with is actually making the GPIO25 output HIGH (produce 3.3V). This can be achieved by making 8th bit of GPIO25_CTRL register 1. In a C program, this can be achieved by,
```C
*(uint32_t *) (0x40014000 + 0x0cc) |= 1 << 8; // Set bit 8 of GPIO25_CTRL register
```
Let's break down the line of C code above. The hex number `0x40014000` is the base address of the GPIO peripheral. And, `0x0cc` is the offset of GPIO25_CTRL register from GPIO base. Thus, `0x400140cc` becomes the actual address of GPIO25_CTRL register. To access the value at that address, `0x400140cc` needs to be converted into a pointer first. This is achieved by type casting the address value into a pointe, `(uint32_t *) (0x40014000 + 0x0cc)`. Now, this pointer can be dereferenced to by adding a leading `*` to access it's value, `*(uint32_t *) (0x40014000 + 0x0cc)`. The remaining operation, `|= 1 << 8`, means to get the existing value of the GPIO25_CTRL register and perform a bitwise OR operation with `1 << 8 = 0b00000000000000000000000100000000`, which sets the 8th bit regardless of its current state.

There are three common operations done on registers,
1. Set a bit - Make any bit of a register 1 - `(uint32_t *) (REGISTER_ADDR) |= 1 << bitLocation`
2. Clear a bit - Make any bit of a register 0 - `(uint32_t *) (REGISTER_ADDR) &= ~(1 << bitLocation)`
3. Flip a bit - Make any bit of a register flip, 0 -> 1 or 1 -> 0 - `(uint32_t *) (REGISTER_ADDR) ^= 1 << bitLocation`

### How an Arm<sup>&copy;</sup> &micro;C boots up?
When an Arm<sup>&copy;</sup> &micro;C boots, it reads a so-called *vector table* from the beginning of Flash. A vector table is a concept common to all Arm<sup>&copy;</sup> &micro;Cs. That is an array of 32-bit addresses of interrupt handlers (event driven functions). First 16 entries are reserved by Arm<sup>&copy;</sup> and are common to all Arm<sup>&copy;</sup> &micro;Cs. The rest of interrupt handlers are specific to the given &micro;C - these are interrupt handlers for peripherals. Simpler &micro;Cs with few peripherals have few interrupt handlers, and more complex &micro;Cs have many.

Every entry in the vector table is an address of a function that &micro;C executes when a hardware interrupt (IRQ) triggers. The exception are first two entries, which play a key role in the &micro;C boot process. Those two first values are: an initial stack pointer, and an address of the boot function to execute (a firmware entry point).

So now we know, that if the firmware is composed in a way that the 2nd 32-bit value in the flash contain an address of the boot function, then the &micro;C will read that address and jump to the boot function when it is powered on.

Unfortunately, RP2040 is an exception here. It doesn't follow this simplified procedure of booting and relies on something much more complicated in order provide some features that other &micro;Cs don't provide out of the box. The [first tutorial](./01_bootupBlinky/README.md) in this series of bare-metal programming deals with this very issue.