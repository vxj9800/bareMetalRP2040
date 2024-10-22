ENTRY(bootStage2);

STACK_SIZE = 0x20000; /* Define Stack Size */

MEMORY
{
    flash(rx) : ORIGIN = 0x10000000, LENGTH = 2048k
    sram(rwx) : ORIGIN = 0x20000000, LENGTH = 256k
}

SECTIONS
{
    .boot2 :
    {
        _sboot2 = .;
        *(.boot2*)
        _eboot2 = .;
        . = . + (252 - (_eboot2 - _sboot2));
        *(.crc*)
    } > flash
    
    .text :
    {
        *(.vector*)
        *(.text*)
    } > flash

    .stack (NOLOAD) :
    {
        _estack = .;
        . = . + STACK_SIZE;
        _sstack = .;
    } > sram
}