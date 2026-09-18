# Roadmap

## Stage 0 — Scaffold (now)
- [x] Repo layout
- [x] Architecture tensions doc
- [x] Decide: padded (simple) vs packed (dense) for v0 allocator → **packed**, 8/10/13-bit
- [x] Define trit syscall ABI (2-bit return convention; see docs/INTERNALS.md §3)

## Stage 1 — Boot (Pentium 4 / IA-32 baseline, 2026-09-18)
- [x] 512-byte boot sector (NASM), prints "G" via BIOS → `src/boot/boot.asm`; CHS load, 3 retries
- [x] Stage2: A20, GDT, 32-bit protected mode, identity paging → `src/boot/stage2.asm`; prints "P4" to VGA
- [x] Handoff to 32-bit C kernel entry (`kmain` in `src/kernel/main.c`, `gcc -m32`)
- [x] QEMU boot verified via VGA+monitor: PM transition, paging, "P4" on screen (see docs/PENTIUM4.md for QEMU quirks)
- [ ] IDT, 8259 PIC remap, 8253/8254 PIT init (interrupts stay OFF until then)
- [ ] BSS clearing in kernel entry (real hardware needs it)
- Note: 8086 real-mode target retired; see docs/PENTIUM4.md and docs/hardware/8086.md (historical appendix)

## Stage 2 — 10-bit cell layer
- [x] `cell.h`: packed 8/10/13-bit get/put (bit-stream across 16-bit words)
- [x] `alloc.h`/`alloc.c`: dynamic bit-width region allocator (first-fit, width-tagged, rewidth via copy) + `alloc_cells_aligned`
- [x] Tests on host: C allocator/DOS/kernel-demo tests pass; Rust mirror `cargo test` 10/10 pass
- [x] 32-bit protected-mode build (`gcc -m32 -ffreestanding -nostdlib`); allocator/trit demo runs in kmain
- [ ] Cross-compile to 16-bit target (ia16-elf-gcc) — deferred; P4 is the baseline, 8086 is historical

## Stage 2b — Rust mirror
- [x] `rust/` crate (`trit`, `cell`, `alloc`), `#![no_std]`, behavior-identical to C
- [ ] Port `dos_compat.c` loader logic to Rust (putchar hook is the only platform bit)
- [ ] Shared C/Rust conformance vectors (see docs/REVIEW.md R-14)

## Stage 3 — Ternary kernel core
- [x] `trit.h`: Kleene logic ops (+ Rust mirror)
- [x] Syscall convention with trit returns (documented; INT 21h shim uses it)
- [ ] Scheduler understands T_UNKNOWN as defer/retry

## Stage 4 — Unix V4 shim
- [ ] V4-like syscall numbers (open/read/write/close/fork/exec/wait/exit)
- [ ] Minimal libc mapping V4 semantics → kernel calls
- [ ] Hello-world V4-style program recompiled against our libc

## Stage 4b — DOS compat (Phase 1)
- [x] `dos_compat.h`: .COM / MZ .EXE loader interface + INT 21h shim provisioned
- [x] Implement MZ relocation parser (`dos_compat.c`: header validation, relocation fixups, flat load_para=0)
- [x] Implement INT 21h subset (09h + 02h real; 3Dh/3Eh/3Fh/40h recognized stubs → T_UNKNOWN; 4Ch exit)
- [x] DOS test program in 8-bit region view (host test: .COM load, MZ reloc, INT 21h — all pass)
- [ ] Real-mode IVT hook so actual DOS binaries can `int 0x21` (needs 8086 bring-up — deferred; P4 baseline uses protected mode; DOS compat is a source/API shim)

## Stage 4d — Docs & review
- [x] `docs/INTERNALS.md`: exact algorithms and ABIs
- [x] `USER_MANUAL.md`: granular human-readable manual (concepts, building, API, FAQ)
- [x] `docs/hardware/8086.md`: PIC/PIT/keyboard/serial/VGA bring-up notes
- [x] `docs/hardware/porting.md`: x86_64/AArch64/RISC-V porting notes (scaffold, not ports)
- [x] `docs/REVIEW.md`: full-stack adversarial review + prioritized fix list

## Stage 4c — Windows 3.1 (Phase 2, later)
- [x] ABI reserved (`win31_load_ne`, `win31_thunk_16` stubs)
- [ ] NE loader, 16-bit protected-mode bring-up, USER/GDI/KERNEL shims

## Stage 5 — Bring-up
- [ ] QEMU boot to kernel shell
- [ ] `help`, `alloc`, `trit` demo commands
- [ ] Document what breaks and why

Each stage is independently testable. We do not attempt a full
bootable 10-bit Unix in one go.

## Stage 6 — Graphics (scoped separately; see docs/GRAPHICS.md)
- [x] `fb.h`/`fb.c`: VGA Mode 13h framebuffer abstraction (host-testable, 8-bit regions)
- [x] `docs/GRAPHICS.md`: honest staging (A=VGA fb REAL, B=tiny software GL EMU, C=x86_64 notes FICTION, Vulkan=Phase 3 fiction)
- [x] `docs/GRAPHICS_COMPAT.md`: kernel compat layer plan (G1–G5 syscalls, 8-bit pixel rule, trit→GL mapping, INT 10h shim)
- [ ] Text console on framebuffer (8x8 font, kernel shell UX)
- [ ] Palette module (`k_palette_set`, ports 0x3C8/0x3C9)
- [ ] Double buffering via `k_fb_present` (rep movsb flip)
- [ ] Tiny software GL subset (glClear/glBegin/glEnd/glVertex, fixed-point rasterizer)
- [ ] QEMU screenshot test of Mode 13h bring-up

## Full-scope vision — "the full nine yards" (planning docs; see docs/FULL_SCOPE.md)

The complete stack scoped end-to-end — firmware, compilers, real GPU
drivers, Vista-era and Debian compat, dual shell — even where it's
aspirational. The haunt, labeled honestly. Implementation stays
sequenced in `docs/FULL_ROADMAP.md` (Phases 0–11 with entry/exit
criteria).

- [x] `docs/FULL_SCOPE.md`: master vision, 7 layers (firmware →
  toolchain → kernel → CPU → graphics → compatibility → shell), every
  item labeled v1-real / staged / epic / research
- [x] `docs/FULL_ROADMAP.md`: Phases 0–11, entry/exit criteria and
  definitions of done, including firmware, self-hosting compiler, and
  shell phases
- [x] `docs/SHELL_SPEC.md`: dual cmd/sh grammar, builtins, scripting,
  kernel interface, Debian-style UX
- [ ] Phase 1 retarget: Pentium 4 as oldest-supported (coordinator
  in progress: protected mode, `docs/PENTIUM4.md` NetBurst notes)
- [ ] Real GPU driver program (Phase 10, epic — multi-year, larger
  than the rest of the OS combined)
- [ ] Vista-era Win32 subset for a named program list (Phase 9, epic —
  Wine-scale)
- [ ] Linux/Debian ELF + syscall subset (Phase 8, staged)
- [ ] Self-hosting cell-aware compiler (Phase 4 epic / Phase 11 research)

Rule: nothing marked epic/research blocks staged work; epics run in
parallel once their entry criteria hold. "No differences from the
original host OS" is a per-program test claim, never a global one.
