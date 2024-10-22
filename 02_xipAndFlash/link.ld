ENTRY(bootStage2);

MEMORY
{
    flash(rx) : ORIGIN = 0x10000000, LENGTH = 2048k
    sram(rwx) : ORIGIN = 0x20000000, LENGTH = 256k
}

SECTIONS
{
    .boot2 :                                    /* Changed */
    {
        _sboot2 = .;
        *(.boot2*)                              /* bootStage2 function */
        _eboot2 = .;
        . = . + (252 - (_eboot2 - _sboot2));    /* Pad zeros, addCounter = addCounter + (252 - sizeOfBoot2) */
        *(.crc*)                                /* 4 Byte CRC32 value */
    } > flash

    .text :                                     /* Added */
    {                                           /* Added */
        *(.text*)                               /* Added */
    } > flash                                   /* Added */
}