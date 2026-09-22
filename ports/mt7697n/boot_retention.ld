/* Preserve 16 bytes across whole-chip resets. The official SDK bootloader
 * lives below 0x20010000 and only uses 0x20040000 as the app handoff SP.
 * Reset_Handler loads __StackTop before its first stack access.
 */
SECTIONS {
  .boot_retained 0x2003fff0 (NOLOAD) : {
    KEEP(*(.boot_retained))
  } > SYSRAM
}
__StackTop = ADDR(.boot_retained);
__StackLimit = __StackTop - SIZEOF(.stack_dummy);
ASSERT(SIZEOF(.boot_retained) == 16, "Boot record must occupy exactly 16 bytes")
ASSERT(__StackLimit >= __bss_end__, "Retained boot record overlaps stack/BSS")
