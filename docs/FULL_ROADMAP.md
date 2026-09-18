# GorpOS — Full Roadmap: Stage 0 to the Full Stack

Phased plan from today's scaffold to the full nine yards. Each phase
lists **entry criteria** (what must be true to start), **exit criteria**
(what must be true to call it done), and **definition of done**. Labels:
v1-real / staged / epic / research, as in `FULL_SCOPE.md`.

---

## Phase 0 — Scaffold & Honest Design (now) — v1-real

- Entry: nothing.
- Exit: repo layout, `ARCHITECTURE.md` tensions doc, packed 8/10/13-bit
  cell layer + allocator + ternary core host-tested, `FULL_SCOPE.md`
  written.
- Done when: `gcc -Wall -Wextra` clean, host tests pass, docs state
  what is real vs stubbed. (Mostly complete.)

## Phase 1 — Pentium 4 Bring-up — staged

- Entry: Phase 0 done; `docs/PENTIUM4.md` microarchitecture notes exist.
- Exit: BIOS → protected mode → paging → C `kmain` on QEMU
  (`-cpu pentium4`); GDT/IDT, PIT tick, serial logging, keyboard IRQ.
- Done when: kernel prints to serial in QEMU P4 emulation and handles
  a timer interrupt without crashing.

## Phase 2 — Kernel Core Services — staged

- Entry: Phase 1 done.
- Exit: preemptive scheduler (T_UNKNOWN = defer/retry), message-passing
  IPC with width-tagged buffers, PCI enumeration, driver model with
  interrupt registration.
- Done when: two processes ping-pong messages, a PCI device is listed,
  and a timer-driven preemption demo runs.

## Phase 3 — Firmware Maturity — staged → epic

- Entry: Phase 1 done.
- Exit (staged): Multiboot-compliant loader, GRUB boot, FAT12/ext2
  second-stage reader.
- Exit (epic): coreboot/UEFI port for one chosen P4 board with DRAM
  init. Per-board effort; educational payoff only.
- Done when (staged): GRUB boots the kernel from ext2.

## Phase 4 — Toolchain: Cross → Self-hosting — staged → epic

- Entry: Phase 0 done (freestanding C suffices to start).
- Exit (staged): `i686-elf-gcc` cross toolchain, NASM, linker scripts,
  kernel builds reproducibly; cell/trit intrinsics as compiler macros.
- Exit (epic): self-hosting C-subset compiler running on the kernel.
- Done when (epic): the kernel rebuilds itself under its own compiler
  and the result boots.

## Phase 5 — Graphics: Framebuffer → Software GL — v1-real → staged

- Entry: Phase 1 done (need protected mode + fb syscalls G1–G5).
- Exit (v1-real): VGA Mode 13h fb + text console + palette +
  double buffering; kernel shell renders here.
- Exit (staged): tiny software GL subset draws a spinning triangle;
  VESA VBE linear framebuffer at higher resolutions.
- Done when: QEMU screenshot shows the triangle; host PPM dumps match.

## Phase 6 — Compatibility I: DOS + Win3.1 + Unix V4 — v1-real → staged

- Entry: Phase 2 done (loader + syscall layer exist).
- Exit (v1-real): `.COM`/MZ `.EXE` loader + INT 21h subset, host-tested.
- Exit (staged): real-mode IVT hook; INT 10h video shim; NE loader +
  16-bit thunks for Win3.1; Unix V4 API shim with hello-world.
- Done when: a real DOS `.COM` runs under QEMU and a V4-style program
  compiles against our libc.

## Phase 7 — Shell: Dual cmd/sh — staged

- Entry: Phase 5 done (console UX exists).
- Exit: `SHELL_SPEC.md` fully implemented: dual grammar, builtins,
  `.bat` + `.sh` scripting, Debian-style UX verbs.
- Done when: the same session runs `dir` and `ls`, pipes work in sh
  mode, `%VAR%` and `$VAR` both expand in their modes.

## Phase 8 — Compatibility II: Linux/Debian Subset — staged

- Entry: Phases 2, 6, 7 done.
- Exit: ELF loader + Linux syscall subset; statically-linked binaries
  run; dynamic linker shim for a named set; Debian-style package verbs
  in the shell map to real installs.
- Done when: a named list of static Linux binaries runs unmodified.

## Phase 9 — Compatibility III: Vista-era Win32 — epic

- Entry: Phases 2, 5, 6 done; maintainers accept this is Wine-scale.
- Exit: frozen Win32 subset (kernel32/user32/gdi32 basics) running a
  *named, tested list* of Vista-era programs; failures documented per
  program, not hand-waved.
- Done when: the list passes, and "no differences" is evaluated per
  program against the list — never claimed globally.

## Phase 10 — Real GPU Driver Program — epic

- Entry: Phase 2 driver model + Phase 5 framebuffer mature; a specific
  P4-era GPU chosen (AGP).
- Exit: PCI/AGP enumeration, MMIO, command submission, IRQ, minimal
  modesetting; 2D accel; one shader compiler.
- Done when: the driver modesets and runs the Phase 5 GL subset on
  real hardware faster than software rendering. (Expected: years.)

## Phase 11 — Research Frontiers — research

- Entry: everything above is stable.
- Items: cell-aware/ternary-native compiler type system; Vulkan on
  packed regions; firmware-level cell support; formal verification of
  the allocator.
- Done when: papers/prototypes, not products. Labeled research so
  nobody mistakes them for roadmap commitments.

---

## Phase ordering rules

1. No phase starts until its entry criteria hold.
2. Epic phases (9, 10) never block staged phases; they run in parallel
   once entries are met.
3. Every phase updates `docs/REVIEW.md` with what broke and why.
4. "No differences from the original host OS" is a per-program test
   claim (Phase 9), never a global claim.
