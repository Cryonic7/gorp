# R-18 bring-up plan — DRAFT (uncommitted, awaiting Jaidon's call)

Closes REVIEW.md R-18: IDT + PIC remap + PIT init, so `sti` stops being a
triple-fault. All register detail comes from docs/INTERRUPTS_REF.md
(avenue 12); this file only adds the implementation order, placement
decisions, and acceptance checks. Independent of R-9 (73-sector load);
that blocks a bigger kernel, this blocks any interrupts at all.

Difficulty label: **v1-real**. 8259 + PIT is the smallest real interrupt
system; local APIC/SMP is later (INTERRUPTS_REF §2).

## Placement decisions

- IDT: `static idt_entry_t idt[256] __attribute__((aligned(8)))` in
  kernel `.bss`. 256 x 8 = 2 KB. Flat identity-mapped paging means the
  symbol address works for `lidt` directly. Deliberately NOT a fixed
  address: SeaBIOS scratch lives at 0x7000-0x8FFF (INTERRUPTS_REF) and
  the stack top is 0x9000 growing down (stage2.asm memory map).
- No TSS needed yet: single ring-0 flat model.

## Build order (kmain)

1. `cli` (already the state on entry from stage2 — keep it until the end).
2. `idt_install()`: fill all 256 entries with a default fault stub
   (writes vector number to VGA, then `hlt`), then install real handlers
   for vectors 0x00-0x1F (CPU exceptions) and 0x20-0x2F (IRQs).
3. `pic_remap(0x20, 0x28)`: ICW1=0x11 both chips, ICW2 offsets,
   ICW3 master=0x04 / slave=0x02, ICW4=0x01, `io_wait()` between writes
   (helpers already in io.h). Re-mask everything to 0xFF/0xFF right
   after — ICW1 clears IMR, so the chips come up unmasked.
4. `pit_init(100)`: divisor = 1193182/100 = 11931; control word 0x36
   (ch0, lobyte/hibyte, mode 3 square wave, binary); two separate 8-bit
   writes to 0x40 — NOT one 16-bit `outw`.
5. `irq_install(0, timer_handler)`: handler increments a volatile tick
   counter and sends non-specific EOI (0x20 to 0x20). Keep the handler
   in C with an asm entry stub that pushes registers and calls it.
6. `pic_unmask(0)`: read-modify-write 0x21, clear bit 0 (mask becomes
   0xFE). Keyboard (IRQ1) stays masked in v1.
7. Spurious guards per INTERRUPTS_REF §1.6: IRQ7 handler checks ISR bit
   7 and returns *without* EOI if clear; IRQ15 handler EOIs the master
   only if the slave ISR bit 7 is clear. Count spurious hits (Linux
   does; a rising count = EOI/wiring bug).
8. `sti` — LAST. Then a `for(;;) hlt` loop with the tick counter
   printed to VGA every 100 ticks (1 second of proof).

## New files

- `src/kernel/idt.h`, `src/kernel/idt.c` — entry struct, `lidt`, stubs.
- `src/kernel/pic.c` — remap, mask/unmask, `pic_send_eoi`.
- `src/kernel/pit.c` — `pit_init(frequency)`.
- `src/kernel/irq.c` (or fold into idt.c) — entry stubs + timer handler.
- Hardware paths compile out under HOST_TEST like the existing code;
  keep `main.c`'s host build working.

## Verification (QEMU, no debug-exit — VGA + monitor)

1. Boot: VGA shows the stage2 "P4" marker, then kernel banner, then
   the tick line advancing ~1/sec. No triple fault after `sti`.
2. Monitor `info pic`: master mask shows IRQ0 unmasked (0xFE... IMR
   readout), offsets 0x20/0x28.
3. Fault-stub proof: temporarily trigger `int $0x3` from kmain before
   `sti`; the stub prints "FAULT 0x03" and halts instead of triple
   faulting. Revert after.
4. Spurious proof: the spurious counters exist and stay 0 during a
   normal 60-second run.

## Acceptance

- [ ] `sti` with PIT at 100 Hz: kernel stays alive, tick counter advances.
- [ ] All 256 IDT entries point at valid code; exceptions 0x00-0x1F
      print their vector on VGA instead of triple-faulting.
- [ ] Spurious IRQ7/IRQ15 handlers installed with ISR-bit checks.
- [ ] `HOST_TEST` build still compiles (hardware paths ifdef'd out).
- [ ] REVIEW.md R-18 checked off (date + commit) when landed.

## Explicitly out of scope for R-18

APIC/SMP (later), keyboard IRQ1 (R-next), LAPIC timer calibration
against PIT (needs the PIT first — which this lands).
