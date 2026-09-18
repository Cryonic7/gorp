# TODO — GorpOS running task list

This is the living checklist. Items move here when they're deferred, and get
checked off (with date + commit) when done. Research avenues live in
`gorp-mind/avenues.md`; this file tracks build work and deferred items.

## Build — bring-up (ordered by dependency)

- [ ] **R-18: IDT + PIC remap + PIT init** — any `sti` before these exist triple-faults. Next bring-up milestone. (Ref: docs/VBE_REF.md not relevant; see docs/PENTIUM4.md + new PIC/APIC research when avenue 12 lands.)
- [ ] **R-9: fix 73-sector kernel load** — 66 sectors work; high LBAs fail on SeaBIOS floppy path. Blocks full kernel handoff in QEMU.
- [ ] **R-19: explicit BSS clearing** in kernel entry (`__bss_start`/`__bss_end`) — QEMU zeroes RAM, real firmware doesn't.
- [ ] **R-3: bounds-check region indices** in C `region_get`/`region_put` (assert in debug at minimum); reconcile failure semantics with Rust mirror.
- [ ] **R-5: deduplicate bit access** — factor `pool_get_abs`/`pool_put_abs` into `cell.h`, use from both `cell.h` and `alloc.c`.
- [ ] **R-7: in-tree test suite** — commit `test_alloc.c`, `test_dos.c`, add `Makefile` with `make test`. (Tests lived in /tmp for v0.1.)
- [ ] **R-8: MZ `min_extra` handling** — warn/decline (`T_UNKNOWN`) when `min_extra > 0`; no stack/BSS setup today.
- [ ] **Allocator 128-byte stride** — P4 L2 lines are 128B (not 64); update stride guidance from research correction.
- [ ] **R-15/R-16: P4 hot-path audit** — branchless cell codec paths, 64B/128B line-split avoidance in packed streams.
- [ ] **R-6: fragmentation stress test** — hammer alloc/free patterns across mixed 8/10/13-bit workloads.
- [ ] **R-13: rewrite ARCHITECTURE.md §1** in present tense (packed); padded design to footnote.
- [ ] **R-14: shared C/Rust conformance vectors** — same test vectors, both languages.
- [ ] **R-21: split RWX segment** — separate text/rodata/data permissions in linker script.

## Research avenues (from gorp-mind/avenues.md)

- [x] 1–8: syscalls, NE, VBE, GPU 2D, filesystem, compilers, firmware, shell grammar (done 2026-09-17)
- [x] 11: License verification sweep for compiler candidates (done 2026-09-18; tcc LGPL-2.1 usable, cproc/QBE ISC+MIT cleared, lcc DISQUALIFIED, SmallerC license unverified→hold)
- [x] 12: PIC/APIC + PIT/HPET programming reference (done 2026-09-18; docs/INTERRUPTS_REF.md — unblocks R-18 bring-up)
- [x] 13: i386 TLS/segmentation deep dive (done 2026-09-18; docs/TLS_I386.md — set_thread_area contract, clone arg map, %gs-why)
- [ ] 14: Voodoo3 driver bring-up plan ← in progress
- [ ] 15: GorpFS v0 full on-disk spec + 8-bit foreign-view translation layer ← in progress
- [ ] 9: Sound (AC'97 / Intel HDA) bring-up notes — untouched
- [ ] 10: USB (UHCI/OHCI) + networking (NE2000/RTL8139) — untouched

## Later / parked

- [ ] Shell implementation from SHELL_SPEC.md + SHELL_GRAMMAR.md
- [ ] DOS INT 21h file I/O (3Dh/3Eh/3Fh/40h) onto k_open/k_read/k_write
- [ ] Windows 3.1 NE loader (Phase 2) per NE_FORMAT.md
- [ ] Linux syscall shim (Phase 8) per SYSCALLS_I386.md
- [ ] x86_64 / AArch64 / RISC-V porting per docs/hardware/porting.md
