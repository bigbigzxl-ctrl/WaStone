/* STM32H563RGT6 — 1MB Flash, 256KB SRAM1
 *
 * Flash:  0x08000000  1024K
 * SRAM1:  0x20000000  256K
 *
 * AEL mailbox at 0x20007F00 (well within SRAM1, 256-byte below 32KB mark).
 * Stack top at 0x20040000 (top of SRAM1).
 */

ENTRY(Reset_Handler)

MEMORY
{
    FLASH (rx)  : ORIGIN = 0x08000000, LENGTH = 1024K
    RAM   (rwx) : ORIGIN = 0x20000000, LENGTH = 256K
}

_estack = ORIGIN(RAM) + LENGTH(RAM);   /* 0x20040000 */

SECTIONS
{
    .isr_vector :
    {
        KEEP(*(.isr_vector))
    } > FLASH

    .text :
    {
        *(.text*)
        *(.rodata*)
        _etext = .;
    } > FLASH

    _sidata = LOADADDR(.data);

    .data : AT ( _etext )
    {
        _sdata = .;
        *(.data*)
        _edata = .;
    } > RAM

    .bss :
    {
        _sbss = .;
        *(.bss*)
        *(COMMON)
        _ebss = .;
    } > RAM

    /* .noinit: not zero-initialized by startup code; survives SYSRESETREQ
     * (SRAM content is preserved across system resets on STM32H563) */
    .noinit (NOLOAD) :
    {
        *(.noinit*)
    } > RAM

    /DISCARD/ :
    {
        *(.ARM.exidx*)
        *(.ARM.extab*)
    }
}
