/* kernel.ld — link GorpOS 32-bit kernel at 0x10000 (Pentium 4 baseline)
 * Build: ld -m elf_i386 -T kernel.ld -o kernel.elf kernel.o
 *        objcopy -O binary kernel.elf kernel.bin
 * kmain is forced first via .text.entry so stage2's jump to 0x10000
 * lands exactly on the entry point.
 */
ENTRY(kmain)
SECTIONS
{
    . = 0x10000;
    .text.entry : { *(.text.entry) }
    .text : { *(.text) }
    .rodata : { *(.rodata*) }
    .data : { *(.data) }
    .bss : { *(.bss) *(COMMON) }
}
