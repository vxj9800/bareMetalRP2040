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

    .data :
    {
        *(.data*)
    } > sram AT > flash     /* "> sram" is the VMA, "> flash" is the LMA */

    .stack (NOLOAD) :
    {
        . = ORIGIN(sram) + LENGTH(sram);
        _sstack = .;
    } > sram

    /* Get LMA and VMA for .data section */
    _sdata = ADDR(.data);               /* Get starting LMA */
    _edata = _sdata + SIZEOF(.data);    /* Get ending LMA */
    _sdataf = LOADADDR(.data);          /* Get starting VMA */
}