# Pentium 4 Hardware Bring-Up Notes (IA-32 Protected Mode)

Target: Pentium 4 / NetBurst, 32-bit protected mode with paging.
See `docs/PENTIUM4.md` for the microarchitecture. This doc covers the
PC/AT hardware programming: PIC, PIT, and the IDT.

**Status:** IDT, PIC remap, and PIT init are NOT yet implemented.
Interrupts remain OFF until they are. (R-18)

## Memory map (protected mode, as set up by stage2)

| Range            | Use                                              |
|------------------|--------------------------------------------------|
| 0x00000–0x003FF  | IVT (real-mode; not used in PM)                  |
| 0x00400–0x004FF  | BIOS data area                                   |
| 0x05000–0x05FFF  | Page directory (4 KB)                            |
| 0x06000–0x06FFF  | Page table (4 KB, identity-maps 0–4 MB)          |
| 0x07C00–0x07DFF  | Boot sector (real-mode only)                     |
| 0x07E00–0x07FFF  | Stage2 (real-mode prologue + PM code)            |
| 0x09000          | Stack top (grows down; PM)                       |
| 0x10000+         | Kernel (`kmain`, linked at 0x10000)               |
| 0xB8000–0xBFFFF  | VGA text framebuffer                             |

**Note:** 0x7000–0x8FFF is SeaBIOS scratch — do NOT place paging
structures there (verified 2026-09-18). See R-20.

## GDT (set up by stage2)

Flat 32-bit segments, base 0, limit 4 GB:
- 0x08: code, execute/read, DPL 0
- 0x10: data, read/write, DPL 0

## IDT (TODO — R-18)

256 entries, 8 bytes each. Interrupt gates for CPU exceptions (0x00–0x1F),
remapped IRQs at 0x20–0x2F. The IDT must exist before `sti`.

Exception stubs (assembly): push error code (or dummy), push vector
number, call C handler, `iret`. The C handler prints the vector and
halts until a real fault-recovery story exists.

## 8259A PIC — remap (TODO)

Same sequence as the 8086 notes (ports 0x20/0x21, 0xA0/0xA1), but the
IDT entries at 0x20–0x2F must point to IRQ stubs. Mask all IRQs until
handlers are installed. EOI: `outb(0x20, 0x20)` (master), plus
`outb(0xA0, 0x20)` for slave IRQs.

```c
// Remap: master base 0x20, slave base 0x28 (see 8086.md for the full sequence)
outb(0x20, 0x11); io_wait();
outb(0xA0, 0x11); io_wait();
outb(0x21, 0x20); io_wait();
outb(0xA1, 0x28); io_wait();
outb(0x21, 0x04); io_wait();
outb(0xA1, 0x02); io_wait();
outb(0x21, 0x01); io_wait();
outb(0xA1, 0x01); io_wait();
outb(0x21, 0xFF); outb(0xA1, 0xFF); // mask all until handlers ready
```

## 8253/8254 PIT — system timer (TODO)

Channel 0, mode 3 (square wave), 100 Hz tick → IRQ0. Same as 8086.md:
`outb(0x43, 0x36)`, divisor `1193182/100` to port 0x40. The IRQ0 handler
sends EOI and increments a tick counter; the scheduler hooks in later.

On P4-era hardware the PIT still exists (the local APIC timer is a
later optimization; not baseline).

## Port I/O in PM

`inb`/`outb` in `src/kernel/io.h` work unchanged in ring 0. The
`io_wait()` (out to 0x80) is for ISA bus settling; harmless on P4.

**QEMU quirk:** After BIOS video calls, port I/O to 0xF4 hangs on
QEMU 9.2.1. Stage2 avoids INT 10h entirely. See docs/PENTIUM4.md.

## A20 (done in stage2)

INT 15h AX=2401, port 0x92 fallback. Verified working 2026-09-18.

## What's NOT here yet

- IDT + exception stubs (R-18)
- PIC remap + IRQ stubs
- PIT init + IRQ0 handler
- Keyboard IRQ1 handler (poll 0x60 for now if needed)
- Local APIC (not baseline; P4 has it, but the 8259A is the bring-up path)
- SMP / Hyper-Threading (not baseline)
