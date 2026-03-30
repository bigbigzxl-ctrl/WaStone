/* STM32U585CIU6 — 2MB Flash, 192KB SRAM1
 *
 * Flash: 0x08000000  2048K
 * SRAM1: 0x20000000  192K   (used here; SRAM2/3 omitted for simplicity)
 *
 * AEL mailbox convention: fixed at 0x20007F00 (well within SRAM1).
 * The linker script places no section there; firmware writes it explicitly.
 */

ENTRY(Reset_Handler)

MEMORY
{
    FLASH (rx)  : ORIGIN = 0x08000000, LENGTH = 2048K
    RAM   (rwx) : ORIGIN = 0x20000000, LENGTH = 192K
}

_estack = ORIGIN(RAM) + LENGTH(RAM);   /* 0x20030000 */

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

    /* LMA for .data initialisation values sits right after .text in flash */
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

    /DISCARD/ :
    {
        *(.ARM.exidx*)
        *(.ARM.extab*)
    }
}
