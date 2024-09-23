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