ENTRY(bootStage2);

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
        . = ORIGIN(sram) + LENGTH(sram);
        _sstack = .;
    } > sram
}