/* scripts/size_measure.ld — minimal Cortex-M0+ linker script for linked-ELF
 * size measurement. Flash at 0x10000000 (256K), RAM at 0x20000000 (256K). */

MEMORY
{
    FLASH (rx) : ORIGIN = 0x10000000, LENGTH = 256K
    RAM   (rwx) : ORIGIN = 0x20000000, LENGTH = 256K
}

ENTRY(Reset_Handler)

SECTIONS
{
    .text : {
        KEEP(*(.vectors))
        *(.text*)
        *(.glue_7)
        *(.glue_7t)
        *(.eh_frame)
        KEEP(*(.init))
        KEEP(*(.fini))
    } > FLASH

    .rodata : {
        *(.rodata*)
    } > FLASH

    .ARM.extab : { *(.ARM.extab*) } > FLASH
    .ARM.exidx : { *(.ARM.exidx*) } > FLASH

    . = ALIGN(4);
    _etext = .;

    .data : AT(_etext) {
        _sdata = .;
        *(.data*)
        . = ALIGN(4);
        _edata = .;
    } > RAM

    .bss : {
        _sbss = .;
        *(.bss*)
        *(COMMON)
        . = ALIGN(4);
        _ebss = .;
    } > RAM

    . = ALIGN(8);
    _estack = ORIGIN(RAM) + LENGTH(RAM);
}
