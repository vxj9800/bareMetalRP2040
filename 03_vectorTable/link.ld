ENTRY(bootStage2);

/* Define Stack Size */
STACK_SIZE = 0x20000;

MEMORY
{
    flash(rx) : ORIGIN = 0x10000000, LENGTH = 2048k
    sram(rwx) : ORIGIN = 0x20000000, LENGTH = 256k
}

SECTIONS
{
    .boot2 :
    {
        *(.boot2*)
        . = ORIGIN(flash) + 252;
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