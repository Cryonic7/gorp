# GorpOS — Full Scope: The Full Nine Yards

The complete vision, bottom to top, unflinching. The user asked for the
whole stack — firmware, compilers, kernel, GPU drivers, Windows
Vista-era and Debian compat, dual shell — "never stop short of your
goal, always build the full stack."

This document scopes it ALL. Every layer carries an honest difficulty
label so the scope guides instead of haunting:

- **v1-real** — buildable now, bounded work, host-testable.
- **staged** — planned, bounded, sequenced after prerequisites.
- **epic** — multi-year, team-scale effort (think Wine, Mesa, LLVM).
  Listed completely, labeled honestly.
- **research** — feasibility unknown; may never resolve.

Related docs: `ARCHITECTURE.md` (core tensions), `GRAPHICS.md` /
`GRAPHICS_COMPAT.md` (graphics staging), `SHELL_SPEC.md` (dual shell),
`FULL_ROADMAP.md` (phased plan with entry/exit criteria),
`docs/PENTIUM4.md` (NetBurst microarchitecture notes, in progress).

---

## Layer 1 — Firmware

**Goal:** own the boot path from power-on to kernel handoff on the
Pentium 4 baseline, with a documented path to UEFI.

**Milestones:**
1. **v1-real:** BIOS boot. 512-byte MBR at 0x7C00 → real-mode loader →
   switch to 32-bit protected mode (GDT, A20, paging) → jump to kernel.
   QEMU-tested (`qemu-system-i386 -cpu pentium4`).
2. **staged:** Multiboot-compliant loader so GRUB can boot the kernel;
   second-stage loader with a real filesystem reader (FAT12 → ext2).
3. **staged:** Option ROM notes: VGA BIOS interaction, why we rely on
   INT 10h in v1 instead of native modesetting.
4. **epic:** Custom UEFI firmware / coreboot port for a chosen P4-era
   board. This means board-specific chipset init (Intel 845/865),
   DRAM training, and Super I/O — months of work per board, and the
   payoff over SeaBIOS is mostly educational.
5. **research:** Firmware-level support for the 10-bit cell model
   (e.g. SMM handlers that understand width-tagged regions). No
   precedent; listed for completeness.

**Dependencies:** CPU layer (P4 boot protocol), Toolchain (assembler).

---

## Layer 2 — Toolchain

**Goal:** a complete build chain that targets the kernel, ending in a
self-hosting compiler that understands the cell model natively.

**Milestones:**
1. **v1-real:** Host cross toolchain: `i686-elf-gcc` binutils+gcc,
   NASM for boot asm, Make/CMake build. Kernel is freestanding C.
2. **staged:** Custom assembler/macro layer for kernel conventions
   (trit return convention, cell accessors as intrinsics).
3. **staged:** Linker scripts for the kernel image layout (real-mode
   trampoline + protected-mode kernel + packed-region metadata).
4. **epic:** Self-hosting C compiler (subset) running ON the kernel —
   a small C frontend → IR → i686 codegen, no libc required. This is a
   multi-year single-person project on its own (cf. the scale of
   TCC, which took years and targets normal 8-bit bytes).
5. **research:** A compiler whose *type system* natively understands
   8/10/13-bit cells and ternary logic — `cell10` as a first-class
   type, `trit` with Kleene semantics checked at compile time,
   width-tagged pointers. No production compiler does this; it is a
   genuine research artifact.

**Dependencies:** Kernel (ABI to target), Firmware (image format).

---

## Layer 3 — Kernel

**Goal:** the alien core — packed cells, ternary logic, scheduler,
IPC, driver model — on the P4 baseline.

**Milestones:**
1. **v1-real:** Packed 8/10/13-bit cell layer + first-fit
   width-tagged allocator (`src/kernel/cell.h`, `alloc.h`) — done,
   host-tested.
2. **v1-real:** Ternary core (`trit.h`, Kleene ops, trit syscall
   convention) — done.
3. **staged:** Protected-mode kernel: GDT/IDT, paging, PIT/PIC,
   keyboard/serial drivers, preemptive scheduler where `T_UNKNOWN`
   means defer/retry.
4. **staged:** IPC: message passing with width-tagged buffers and
   trit-aware delivery receipts.
5. **staged:** Driver model: bus enumeration (PCI on P4!), device
   handles, interrupt registration — the foundation the GPU program
   (Layer 5) needs.
6. **epic:** SMP on Hyper-Threading P4s (APIC, per-CPU state, TLB
   shootdown). Real but large; correctly labeled epic, not v1.
7. **research:** Formal verification of the packed allocator's
   bit-math (the cross-word merge cases are exactly where bugs
   hide — see `docs/REVIEW.md`).

**Dependencies:** CPU (P4 protected mode), Firmware (handoff),
Toolchain (freestanding C).

---

## Layer 4 — CPU: Pentium 4 baseline, forward path

**Goal:** Pentium 4 / NetBurst as oldest-supported; microarchitecture
notes kept isolated for future rewrite; documented forward path.

**Milestones:**
1. **v1-real:** P4 baseline: IA-32, protected mode + paging, SSE2
   assumed, QEMU `-cpu pentium4`. See `docs/PENTIUM4.md`.
2. **staged:** P4-specific optimization notes (64-byte cache lines vs
   packed bit-streams, store-forwarding limits, branch mispredict
   cost on the 20-stage pipeline) — kept as *notes*, not baked in.
3. **staged:** Microarchitecture rewrite hook: the notes in
   `docs/PENTIUM4.md` are structured so a future rewrite of the
   P4 target (or a new oldest-supported CPU) only touches
   `src/arch/p4/`, never generic kernel code.
4. **epic:** x86_64 long-mode port (new boot path, new paging, SYSCALL).
5. **epic:** AArch64 and RISC-V ports (the portable C core — cells,
   allocator, trit — moves; everything else is rewritten).

**Dependencies:** Firmware (per-arch boot), Toolchain (per-arch
codegen).

---

## Layer 5 — Graphics: the real GPU program

**Goal:** real GPU drivers and a real graphics stack. This is the
largest single epic in the project. Staged honestly underneath it.

**Milestones:**
1. **v1-real:** VGA Mode 13h framebuffer + text console
   (`src/kernel/fb.h`, `fb.c` — done, host-tested). QEMU-faithful.
2. **staged:** VESA VBE linear framebuffer (higher resolutions via
   INT 10h AX=4F02h, bank switching avoided via LFB).
3. **staged:** Tiny software OpenGL subset (glClear/glBegin/glEnd/
   glVertex/glColor, fixed-point triangle rasterizer) — "spinning
   triangle" milestone. See `GRAPHICS.md` Stage B.
4. **epic:** Real GPU driver program: PCI/AGP enumeration, MMIO BAR
   mapping, command submission, IRQ handling, modesetting (a minimal
   DRM/KMS-style driver), 2D acceleration. Then a shader compiler
   for one GPU generation. Each of those is a person-year or more;
   together this is larger than the rest of the OS combined. The
   honest reference point: the Linux DRM subsystem is ~2M lines.
5. **epic:** Real OpenGL on the real driver (Mesa port or native
   implementation) — depends on the driver program above.
6. **research:** Vulkan: explicit memory heaps and queue families
   mapped onto width-tagged packed regions. The allocation models
   are actively hostile to each other; scoped as Phase 3, may never
   happen. A `vk_min.h` stub shim is the most that is planned.

**Dependencies:** Kernel driver model (Layer 3.5), CPU (P4 AGP/PCI),
Firmware (VGA BIOS / GOP later).

---

## Layer 6 — Compatibility: run the world's software

**Goal:** foreign binaries run as if on their home OS. Staged from
honest shims to Wine-scale epics. Nothing here is binary-magic; every
stage is loader + ABI translation.

**Milestones:**
1. **v1-real:** DOS Phase 1: `.COM` / MZ `.EXE` loader + INT 21h
   subset (09h, 02h, 4Ch; file ops recognized) — implemented,
   host-tested (`src/kernel/dos_compat.c`).
2. **staged:** DOS Phase 1b: real-mode IVT hook so actual DOS
   binaries can `int 0x21`; INT 10h video shim (00h/0Ch/0Dh/13h)
   through the fb layer.
3. **staged:** Windows 3.1 (NE executables, 16-bit protected mode,
   USER/GDI/KERNEL thunks) — ABI reserved, large follow-up.
4. **staged:** Unix V4 (1975) API shim: V4-style syscalls recompiled
   against our libc (API-level, never binary — PDP-11 vs P4).
5. **staged:** Linux/Debian program: ELF loader + Linux syscall ABI
   *subset* (open/read/write/close/mmap/brk/exit/sigaction…),
   targeting statically-linked binaries first, then a dynamic
   linker shim. Debian UX via the dual shell (Layer 7).
6. **epic:** Windows Vista-era Win32: the full API surface through
   Vista — thousands of APIs, WDDM display model, UAC, SxS
   assemblies, COM, registry. This is the Wine project (30 years,
   hundreds of contributors) re-targeted at an alien kernel with a
   10-bit cell model. Scoped completely, labeled epic without
   apology. Realistic near-term slice: a frozen subset of Win32
   (kernel32/user32/gdi32 basics) for a *named list* of Vista-era
   programs, tested one by one.

**Dependencies:** Kernel (loader, syscall layer, driver model for
WDDM-class work), Toolchain (format parsers), Graphics (for any GUI
program).

---

## Layer 7 — Shell: one shell, two mother tongues

**Goal:** an integrated shell that speaks both Windows `cmd`-style
and Linux `sh`-style fluently, with Debian-style UX on the Linux
side. Full spec in `SHELL_SPEC.md`.

**Milestones:**
1. **v1-real:** Kernel demo shell (`help`, `alloc`, `trit`) on the
   framebuffer text console.
2. **staged:** Dual-grammar parser: `cmd` builtins (`dir`, `copy`,
   `del`, `type`, `cd` with `\` paths, `%VAR%`, `if errorlevel`)
   and `sh` builtins (`ls`, `cp`, `rm`, `cat`, pipes, `$VAR`,
   globbing) in one binary, auto-detecting or explicitly switched.
3. **staged:** Scripting: batch (`.bat`) + shell (`.sh`) execution,
   with a documented semantics table where they differ.
4. **staged:** Debian-style UX: `apt`-flavored package verbs mapped
   to our software set, man-style help, `/bin` `/etc` virtual
   layout over our namespace.
5. **epic:** Full `cmd.exe` + POSIX `sh` fidelity including
   extensions, job control, and terminal semantics — each is
   independently a large spec; both together is the epic label.

**Dependencies:** Kernel (syscalls, console), Compatibility (which
foreign programs the shell can launch).

---

## Cross-layer truth table

| Item | Label | Why the label |
|---|---|---|
| Boot to protected-mode kernel on P4 | staged | Bounded; standard OS-dev work |
| Packed 8/10/13-bit allocator | v1-real | Done, tested |
| Ternary kernel core | v1-real | Done (scheduler deferral: staged) |
| Self-hosting cell-aware compiler | epic/research | TCC-scale + novel type system |
| Custom firmware per board | epic | Per-board months, educational payoff |
| Real GPU driver + shader compiler | epic | Larger than the rest combined |
| Real OpenGL / Vulkan | epic/research | Depends on driver; Vulkan hostile to cell model |
| DOS .COM/.EXE | v1-real | Done (host-tested) |
| Win3.1 NE | staged | Large but bounded |
| Vista-era Win32, "no differences" | epic | Wine-scale, 30-yr reference |
| Debian ELF + syscall subset | staged | Bounded subset; "no differences" is epic |
| Dual cmd/sh shell | staged | Parser work; full fidelity is epic |
| Unix V4 API shim | staged | ~40 syscalls, recompiled sources |

Nothing in this document is promised for v1 except the rows marked
v1-real. Everything else is sequenced in `FULL_ROADMAP.md`.
