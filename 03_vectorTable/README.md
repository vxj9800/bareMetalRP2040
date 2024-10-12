## 3. Boot Like Any Other Arm<sup>&copy;</sup> &micro;C
The discussions provided in Tutorials 1 and 2 handles the unorthodox boot-up process of RP2040 where the primary goal was to set up an external Flash appropriately so that the processor can execute user application from it. Though this approach is not too common, more and more &micro;Cs are switching to this model because it gives flexibility to the user to add a Flash chip with the size that they require. Moreover, from manufacturing perspective, a &micro;C without onboard Flash tends to be much cheaper and smaller in size.

Regardless of the fact that RP2040 requires the XIP setup properly at the boot time, the code in previous tutorial reached a state where the RP2040 can now behave exactly like any other &micro;C with onboard Flash, with minimal changes in the code. The goal of this tutorial is to discuss the classic Arm<sup>&copy;</sup> boot-up process and contrast it with RP2040's boot-up and see how it behaves in a similar way moving forward.

Reading Material:
- [ARMv6-M exception model](https://cdn.hackaday.io/files/1770827576276288/DDI0419E_armv6m_arm.pdf#page=191)

### Arm<sup>&copy;</sup> &micro;C Boot-Up Process
An extremely crude overview of Arm<sup>&copy;</sup> &micro;C boot-up process was discussed on the main page of the repository. However, a huge amount of finer details were skipped in that discussion. For example, it states that *the Arm<sup>&copy;</sup> &micro;C reads a so-called *vector table* from the beginning of Flash at the boot-up*. However, even before reading the vector table, the &micro;C sets the address of the vector table to zero, sets values of some important registers, etc., which was not explained there. So, let's take a look at the pseudo-code of the Arm<sup>&copy;</sup> &micro;C reset behavior, i.e. things an Arm<sup>&copy;</sup> &micro;C does immediately after powering on, available in [ARMv6-M Reset Behavior](https://cdn.hackaday.io/files/1770827576276288/DDI0419E_armv6m_arm.pdf#page=195).
```C
// TakeReset()
// ============
TakeReset()
    VTOR = Zeros(32);
    for i = 0 to 12
        R[i] = bits(32) UNKNOWN;
    bits(32) vectortable = VTOR;
    CurrentMode = Mode_Thread;
    LR = bits(32) UNKNOWN; // Value must be initialised by software
    APSR = bits(32) UNKNOWN; // Flags UNPREDICTABLE from reset
    IPSR<5:0> = Zeros(6); // Exception number cleared at reset
    PRIMASK.PM = '0'; // Priority mask cleared at reset
    CONTROL.SPSEL = '0'; // Current stack is Main
    CONTROL.nPRIV = '0'; // Thread is privileged
    ResetSCSRegs(); // Catch-all function for System Control Space reset
    for i = 0 to 511 // All exceptions Inactive
        ExceptionActive[i] = '0';
    ClearEventRegister(); // See WFE instruction for more information
    SP_main = MemA[vectortable,4] AND 0xFFFFFFFC<31:0>;
    SP_process = ((bits(30) UNKNOWN):'00');
    start = MemA[vectortable+4,4]; // Load address of reset routine
    BLXWritePC(start); // Start execution of reset routine
```
Let's try to dissect this pseudo-code one line at a time.

#### `VTOR`
This is a 32-bit register that holds the offset of the Interrupt Vector Table. [ARMv6-M Manual](https://cdn.hackaday.io/files/1770827576276288/DDI0419E_armv6m_arm.pdf#page=231) states that *the number of bits in the `VTOR` is IMPLEMENTATION DEFINED*. [In case of RP2040](https://datasheets.raspberrypi.com/rp2040/rp2040-datasheet.pdf#page=87), upper 24 bits of `VTOR` can be written. Thus, for RP2040, the vector table must be aligned at 256 bytes, i.e. the offset can be of the form `0xXXXXXX00`. Note that it is being set to zero in the boot-up routine above, meaning that the vector table is expected to be at `0x00000000`. Furthermore, this value is also stored in a 32-bit variable called `vectortable` in line 4. Try to find out from the RP2040 datasheet where the bootrom starts at and what exactly it contains.

#### Processor Core Registers
These are the registers using which the instructions are actually carried out. [ARMv6-M contains](https://cdn.hackaday.io/files/1770827576276288/DDI0419E_armv6m_arm.pdf#page=186) thirteen general-purpose 32-bit registers, `R0`-`R12`, and additional three 32-bit registers that have special names and usage models:

- Stack Pointer **(SP)**: This register contains address of the top of the stack at all times. *Stack* generally refers to a data structure where the only way to add or remove items is from the top, meaning the last item added is the first one to be removed, following a Last In First Out (LIFO) principle. This idea is implemented in almost every processor architecture at the hardware level, and is used to hold *automatic* variables and context/parameters across function calls.
- Link Register **(LR)**: This register stores address of the instruction from where a subroutine/function was called. When a function is called, the value of the program counter is stored in the link register and this value is restored into the program counter when the function exits. This is set to an `UNKNOWN` value at line 6 since there is no instruction to go back to once *Reset* finishes.
- Program Counter **(PC)**: This register always contains the address of the instruction that is supposed to be executed next. 0th bit of this register indicates whether the processor is supposed to interpret the instructions as Arm or Thumb. Since ARMv6-M only supports Thumb instruction set, this bit must be set to 1 when PC is written to.

Note that lines 2-3 in the pseudo-code above says that the core registers will contain `UNKNOWN` values. This is because the state of the core registers is unknown and don't matter when the &micro;C is reset.

#### Execution Mode
[ARMv6-M executes](https://cdn.hackaday.io/files/1770827576276288/DDI0419E_armv6m_arm.pdf#page=30) instructions in two modes, 1. Thread Mode and 2. Handler Mode. Thread mode is the fundamental mode for instruction execution in ARMv6-M and is selected on reset as seen in line 5. All exceptions/interrupts execute in Handler mode, with the `Reset_Handler` being the only exception. This distinction is provided to segregate the privileges gained by different parts of the code.

#### Special Purpose Registers
##### Program Status Registers
[ARMv6-M contains](https://cdn.hackaday.io/files/1770827576276288/DDI0419E_armv6m_arm.pdf#page=186) three special-purpose program status registers,
1. Application Program Status Register **(APSR)**: This register is closely tied together with the Arithmetic Logic Unit (ALU), that performs basic math operations. This register contains four bits to indicate if the instruction result was a **N**egative value, result was **Z**ero, result ended with a **C**arry or the value O**v**erflowed. Since there was no Arithmetic operation conducted at the Reset, the value of this register is `UNKNOWN` as shown in line 7.
2. Interrupt Program Status Register **(IPSR)**: While in handler mode, it contains the number corresponding to the exception/event that occurred. It is zero otherwise. Since `Reset_Handler` is not an exception, as discussed previously, this register is set to zero in line 8.
3. Execution Program Status Register **(EPSR)**: This register dictates whether the processor is interpreting instructions as Arm or Thumb instructions. Since ARMv6-M supports only Thumb instruction set, this register cannot be written to and it always reads 0.

##### PRIMASK
This is a 1 bit wide interrupt mask register. When set, it blocks all interrupts apart from the non-maskable interrupt (NMI) and the hard fault exception, more on these in [Vector Table](#interrupt-vector-table-or-vector-table). If the [`PM`](https://cdn.hackaday.io/files/1770827576276288/DDI0419E_armv6m_arm.pdf#page=188) bit is set then it prevents activation of all exceptions with configurable priority. Note that this register is being cleared in line 9 above, meaning that other interrupts/exceptions are allowed to occur even right after reset.

##### CONTROL
This is a 2 bit wide register that controls teh execution privilege and the type of stack pointer used. If the `nPriv` bit is set then the execution has unprivileged (limited) access to memory in the Thread mode. If the `SPSEL` bit is set then a process stack pointer is used as the Stack Pointer. In lines 10-11, note that the Thread Mode is set to have privileged access and the Stack Pointer being used is the main stack pointer. These ideas are more relevant to the concept of Real-Time Operating System (RTOS), and are not discussed in further detail here.

#### System Control Space
This comprises a big set of 32 bit registers that handle configuration, status reporting and control of the rest of the processor. These registers are divided into following [groups in ARMv6-M](https://cdn.hackaday.io/files/1770827576276288/DDI0419E_armv6m_arm.pdf#page=227):
- System control and identification
- CPUID processor identification space
- System configuration and status
- System timer, `SysTick`
- Nested Vectored Interrupt Controller (NVIC)
- System debug

In line 12, all the registers in SCS are being set to their default value. The `ResetSCSRegs` function performs this operation.

#### Interrupt Vector Table or Vector Table
As discussed previously, Interrupt Vector Table is an array of 32-bit addresses. One of these addresses is a stack pointer and the rest are interrupt handlers (functions that are triggered when specific [events/exceptions](https://cdn.hackaday.io/files/1770827576276288/DDI0419E_armv6m_arm.pdf#page=191) occur). [First 16 entries (0 - 15)](https://cdn.hackaday.io/files/1770827576276288/DDI0419E_armv6m_arm.pdf#page=192) are reserved by Arm<sup>&copy;</sup>. Depending on the architecture, some of those 16 may be supported and some may not. All the entries after these are known as External Interrupt. These are implemented by the manufacturer of the &micro;C and are generally tied to some peripheral. The list of external interrupts implemented in RP2040 are available [here](https://datasheets.raspberrypi.com/rp2040/rp2040-datasheet.pdf#page=61).

When any of the exceptions are triggered, they go into a pending state. At every clock cycle, processor finds the exceptions whose pending bit is set, it finds the exception that has the highest priority and calls associated handler by reading the vector table (processor finds the location of vector table from `VTOR`). Once the handler is called, it is the handler's job to clear the pending bit, otherwise it'll be called repeatedly by the processor. In lines 13-14, pending bits of all the exceptions is being cleared out so that the user program in Flash can start in a known state.

Since Cortex-M0+ processors are designed to be energy-efficient, the ARMv6-M architecture provides functionality to put the processor into Sleep, Deep Sleep and Deep Sleep with SRPG states. This is achieved using [Wait For Event (`WFE`) and Send Event (`SEV`)](https://cdn.hackaday.io/files/1770827576276288/DDI0419E_armv6m_arm.pdf#page=209) instructions. At line 15 above, the registers associated with these events are cleared out so that the processor state doesn't change during boot up. If you are interested in power management of RP2040 then take a look at [Section 2.11 of the datasheet](https://datasheets.raspberrypi.com/rp2040/rp2040-datasheet.pdf#page=163).

Finally it is time to start executing the code. Processor needs two pieces of information to do this, 1. Value of the **Stack Pointer** and 2. Pointer to **Reset Handler**. Regardless of the architecture version, the first entry in vector table is assumed to be the value of the Stack Pointer and the second entry is assumed to be the address of the Reset handler. The location of the vector table was already stored in `vectortable` variable at line 4. At line 16, the 4 Byte (32 bit) value of the **Stack Pointer** is copied from the first entry in the vector table and stored as the Main Stack Pointer since the access type at boot up is privileged. Also note the bit-wise `AND` operation being performed with this value. It makes sure that the Stack Pointer is 4 Byte aligned. At line 17, the value of the Process Stack Pointer is expected to be `UNKNOWN` since this should be set by the user code later on. At line 18, the value of second entry in the vector table (**Reset Handler**) is copied into a variable called `start`. And the processor performs a branch-less jump (copies value of `start` into **PC** register) to `start` at line 18. As discussed previously, **PC** contains the location of the next instruction that would execute. Now that the address of **Reset Handler** is copied into **PC**, the `reset` handler provided by the user will execute.

Let's take a moment to ponder upon the ton of information just provided. This information dump also sets up a base for run-time execution topics like context switching, interrupts, power management, etc. So, it is very important that you spend some time understanding the terminology used here and reading up on the reference material provided.

### RP2040's Boot-up Process