# Porting Notes — x86_64, AArch64, RISC-V

Status: scaffold + honest notes. No full ports exist. The design goal is
that the *interesting* part of GorpOS (packed cells, dynamic bit-width
allocator, ternary logic) is portable C, while boot + I/O + interrupts
are per-architecture.

## What ports cleanly (no changes expected)

- `src/kernel/cell.h` — pure bit math, no arch assumptions.
- `src/kernel/alloc.h` / `alloc.c` — pure C99, only needs `stdint.h`.
  The O(n²) first-fit is fine on all three; revisit for large pools.
- `src/kernel/trit.h` — pure logic.
- `src/kernel/dos_compat.c` loader *logic* (MZ parsing, relocations) is
  portable; the *execution* of loaded DOS binaries is 8086-only.

## What must be rewritten per arch

| Layer            | x86_64                        | AArch64                          | RISC-V (rv64)                    |
|------------------|-------------------------------|----------------------------------|----------------------------------|
| Boot             | UEFI or multiboot2; enter long mode via a stub (or use Limine) | UEFI or bare-metal stub at EL2/EL1; set up MMU page tables | OpenSBI + SBI HSM; supervisor binary, `sret` not `iret` |
| Early console    | serial 0x3F8 still exists on most emulators; or framebuffer | MMIO UART (PL011 0x09000000 on QEMU virt; NOT port I/O) | MMIO 16550 UART (QEMU virt 0x10000000) |
| Interrupts       | IDT + LAPIC/IOAPIC, not 8259A | GICv2/v3, exception vectors VBAR_EL1 | PLIC + CLINT, `stvec`/`scause` |
| Port I/O         | `inb`/`outb` still exist      | **do not exist** — replace `io.h` with MMIO (`*(volatile uint32_t*)addr`) | **do not exist** — MMIO only |
| Timers           | LAPIC timer / HPET / TSC      | generic timer (CNTVCT_EL0)       | `mtime`/`mtimecmp` via SBI or MMIO |

## Concrete porting tasks (per arch)

1. Replace `src/kernel/io.h` with an MMIO variant where needed.
2. Replace `src/boot/boot.asm` with the arch's boot stub.
3. Write arch `early_console_putc` (UART addresses above).
4. Interrupt/exception setup per the table above.
5. `kmain` stays the same — it only calls `KPUTS`, allocator, trit.

## What we deliberately do NOT promise

- Running DOS `.COM` binaries on AArch64/RISC-V: the loader parses,
  but 8086 machine code won't execute. That would need CPU emulation
  (a separate, large project).
- Cycle-accurate performance of packed cells on any arch: the packing
  overhead is the experiment, not a bug.

## Suggested order if you port

x86_64 first (closest to 8086: port I/O and serial survive), then
RISC-V (clean MMIO model, great docs), then AArch64 (GIC is the most
annoying of the three interrupt controllers).
