// main.c — GorpOS kernel entry, Pentium 4 (IA-32) baseline
//
// TARGET BUILD (32-bit protected mode, paging on, identity-mapped):
//   gcc -m32 -ffreestanding -nostdlib -c main.c -o kernel.o
//   ld -m elf_i386 -T kernel.ld -o kernel.elf kernel.o
//   objcopy -O binary kernel.elf kernel.bin
// Entered by stage2's far jump to 0x10000 with flat segments,
// interrupts DISABLED (no IDT yet — see ROADMAP).
//
// HOST BUILD (logic test only):
//   gcc -DHOST_TEST main.c alloc.c -o ktest
// Hardware paths (VGA, port I/O, debug-exit) compile out under HOST_TEST.

#include <stdint.h>
#include "trit.h"
#include "cell.h"
#include "alloc.h"

#ifndef HOST_TEST
#include "io.h"   // port I/O still valid on Pentium 4

// ---- VGA text console (0xB8000, identity-mapped) ----
#define VGA_COLS 80
#define VGA_ROWS 25
static volatile uint16_t *const VGA = (volatile uint16_t *)0xB8000;
static uint16_t vga_pos = 0;

static void vga_putc(char c) {
    if (c == '\n') {
        vga_pos = (vga_pos / VGA_COLS + 1) * VGA_COLS;
    } else {
        VGA[vga_pos++] = (uint16_t)(0x0F00 | (uint8_t)c);
    }
    if (vga_pos >= VGA_COLS * VGA_ROWS) vga_pos = 0; // wrap; no scroll yet
}

static void vga_puts(const char *s) {
    while (*s) vga_putc(*s++);
}
#define KPUTS vga_puts
#define KPUTC vga_putc

// QEMU isa-debug-exit helper removed: the debug-exit port hangs after
// BIOS video calls on QEMU 9.2.1, and 8-bit OUT is mis-emulated.
// Verification is done via VGA text + monitor instead (see build.sh).
#else
#include <stdio.h>
#define KPUTS(s) fputs(s, stdout)
#define KPUTC(c) putchar(c)
#endif

// 4K words of kernel heap for the packed allocator (8KB)
#define KPOOL_WORDS 4096
static uint16_t kpool[KPOOL_WORDS];
static allocator_t kalloc;

static void put_hex16(uint16_t v) {
    const char *h = "0123456789ABCDEF";
    for (int i = 12; i >= 0; i -= 4) KPUTC(h[(v >> i) & 0xF]);
}

// Demo: allocate one region of each width, poke cells, show trit logic.
static void demo_cells(void) {
    int r8  = alloc_cells(&kalloc, WIDTH_8, 4);
    int r10 = alloc_cells(&kalloc, WIDTH_10, 4);
    int r13 = alloc_cells(&kalloc, WIDTH_13, 4);
    KPUTS("alloc ids: ");
    put_hex16((uint16_t)r8); KPUTC(' ');
    put_hex16((uint16_t)r10); KPUTC(' ');
    put_hex16((uint16_t)r13); KPUTC('\n');

    region_put(&kalloc, r10, 0, 0x3FF);
    region_put(&kalloc, r10, 1, 0x155);
    KPUTS("10-bit cells: ");
    put_hex16(region_get(&kalloc, r10, 0)); KPUTC(' ');
    put_hex16(region_get(&kalloc, r10, 1)); KPUTC('\n');

    trit_t t = trit_and(T_TRUE, T_UNKNOWN);
    KPUTS("true AND unknown = ");
    KPUTC(t == T_TRUE ? 'T' : t == T_FALSE ? 'F' : 'U');
    KPUTC('\n');
}

#ifdef HOST_TEST
int main(void) {
    alloc_init(&kalloc, kpool, KPOOL_WORDS);
    KPUTS("GorpOS kernel demo (HOST_TEST)\n");
    demo_cells();
    KPUTS("done\n");
    return 0;
}
#else
// Real entry: first byte of the kernel image (see kernel.ld).
// Interrupts are OFF; no IDT yet. Never returns.
__attribute__((section(".text.entry")))
void kmain(void) {
    alloc_init(&kalloc, kpool, KPOOL_WORDS);
    KPUTS("\nGorpOS 32-bit kernel up (P4 baseline)\n");
    demo_cells();
    KPUTS("halting\n");
    for (;;) {
        __asm__ volatile("hlt");
    }
}
#endif
