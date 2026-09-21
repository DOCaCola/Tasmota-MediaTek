/* YLXD01YL application-only link layout. Section placement follows LinkIt
 * v0.10.21 mt7687_flash.ld. No bootloader, radio, OTA, or settings region
 * is allocated here. Bootloader jumps to 0x10071001 with SP=0x20040000.
 * Verified against the owner's dump; runtime boot is still untested.
 */
ENTRY(Reset_Handler)
MEMORY {
    XIP_CODE (arx) : ORIGIN = 0x10071000, LENGTH = 0x00130000
    TCMRAM (arw) : ORIGIN = 0x00100000, LENGTH = 0x00010000
    SYSRAM (arw) : ORIGIN = 0x20000000, LENGTH = 0x00040000
}
__FLASH_segment_start__ = ORIGIN( XIP_CODE );
__FLASH_segment_end__ = __FLASH_segment_start__ + LENGTH( XIP_CODE );

__SRAM_segment_start__ = ORIGIN( SYSRAM );
__SRAM_segment_end__ = __SRAM_segment_start__ + LENGTH( SYSRAM );


/* Define output sections */
SECTIONS
{
    . = ORIGIN(XIP_CODE);
    .text :
    {
        _text_start = .;
        KEEP(*(.init))
        KEEP(*(.fini))

        *(.text*)
        *(.text)
        *(.rodata*)
        _text_end = .;
        . = ALIGN(4);
    } > XIP_CODE

    . = ALIGN(4);
    __etext = .;
    . = ALIGN(4);

    . = ALIGN(4);
    .data :
    {
        __data_start__ = .;
        _data_start = .;
	    KEEP(*(.isr_vector))
        . = ALIGN(4);
        *(.data)
        *(.data*)

        . = ALIGN(4);
        /* preinit data */
        PROVIDE (__preinit_array_start = .);
        KEEP(*(.preinit_array))
        PROVIDE (__preinit_array_end = .);

        . = ALIGN(4);
        /* init data */
        PROVIDE (__init_array_start = .);
        KEEP(*(SORT(.init_array.*)))
        KEEP(*(.init_array))
        PROVIDE (__init_array_end = .);


        . = ALIGN(4);
        /* finit data */
        PROVIDE (__fini_array_start = .);
        KEEP(*(SORT(.fini_array.*)))
        KEEP(*(.fini_array))
        PROVIDE (__fini_array_end = .);

        _data_end = .;
       __data_end__ = .;

    } > SYSRAM AT> XIP_CODE

    . = ALIGN(4);
    .bssRandom (NOLOAD) :
    {
        __bssrandom_start__ = .;
        *(.bssRandom)
        __bssrandom_end__   = .;

    } > TCMRAM

    . = ALIGN(4);
    __ramtext_load__ = LOADADDR(.ramTEXT);

    . = ALIGN(4);
    .ramTEXT :
    {
        __ramtext_start__ = .;
        KEEP(*(.ramTEXT))
        __ramtext_end__ = .;

    } > TCMRAM AT> XIP_CODE

    . = ALIGN(4);
    .tcmBSS (NOLOAD) :
    {
        __tcmbss_start__ = .;
        *(.tcmBSS)
        __tcmbss_end__   = .;

    } > TCMRAM

    . = ALIGN(4);
    .bss :
    {
        __bss_start__ = .;
        *(.bss)
        *(COMMON)
        *(.bss*)
        . = ALIGN(4);
        __bss_end__ = .;
    } > SYSRAM
    . = ALIGN(4);

    /* .stack_dummy section doesn't contains any symbols. It is only
     * used for linker to calculate size of stack sections, and assign
     * values to stack symbols later */

    .stack_dummy :
    {
        PROVIDE ( __end__ = . );
        PROVIDE ( end = . );
        *(.stack)
    } > SYSRAM

    .ARM.extab :
    {
        *(.ARM.extab* .gnu.linkonce.armextab.*)
    } > XIP_CODE

    .ARM.exidx :
    {
        __exidx_start = .;
        *(.ARM.exidx* .gnu.linkonce.armexidx.*)
        __exidx_end = .;
    } > XIP_CODE

    __StackTop = ORIGIN(SYSRAM) + LENGTH(SYSRAM);
    __StackLimit = __StackTop - SIZEOF(.stack_dummy);
    PROVIDE(__stack = __StackTop);
    /* Check if data + stack exceeds RAM limit */
    ASSERT(__StackLimit >= __bss_end__, "region RAM overflowed with stack")

}

ASSERT((Reset_Handler & 0xfffffffe) == 0x10071000, "Stock bootloader entry mismatch")
