# GorpOS — Full-Stack Work Proposal

**Status**: synthesis document. It turns `FULL_SCOPE.md` (the vision)
and `RESEARCH.md` (the evidence) into an executable program of work,
mapped phase-by-phase onto `FULL_ROADMAP.md` Phases 0–11.

---

## 1. Problem statement

Build, from first principles and for the learning value, an operating
system that is *deliberately alien*: a packed 8/10/13-bit cell memory
model on a Pentium 4 baseline, kernel-native Kleene ternary logic, and
staged compatibility shims (DOS → Win3.1 → Vista-era Win32 subset →
Linux/Debian subset) plus a dual cmd/sh shell — with every layer's
honest difficulty label, so the project teaches at every stage and
never pretends to be what it isn't.

**Non-goals** (stated up front so they can't become scope creep):
global "no differences" compatibility claims; real GPU drivers before
Phase 10; ternary *hardware*; 10-bit *native* execution (the cell
model is virtual by construction — RESEARCH §2).

---

## 2. Architectural decisions (with rationale)

| # | Decision | Rationale (→ RESEARCH) |
|---|----------|------------------------|
| D1 | **Pentium 4 / NetBurst as oldest-supported CPU** (IA-32, protected mode + paging as the steady state) | Real hardware target with full docs; SSE2 baseline guaranteed (§1); deep-pipeline quirks give the "rewrite the microarch later" hook the user wants. 8086 kept as historical appendix only. |
| D2 | **BIOS boot → own loader** (not GRUB-first) | Deliberate deviation from community advice (§10) for the learning goal; GRUB/Multiboot becomes Phase 3 staged. |
| D3 | **Packed (not padded) 8/10/13-bit cells** via shift/mask bit-streams | Density over speed; the allocator hides the cost; 13-bit exists to stress the packing logic (§2). |
| D4 | **Width-tagged region allocator, first-fit, rewidth-by-copy** | Width immutable per region keeps packed math tractable; rewidth is explicit and auditable. |
| D5 | **Kernel-native Kleene trit; binary bridge is explicit policy** | History shows ternary fails as hardware but survives as convention (§3: SQL NULL). `T_UNKNOWN` = defer/retry in scheduler; `trit_to_bool` policy documented, never implicit. |
| D6 | **Foreign binaries always see a strict 8-bit view** | Win32/Linux/DOS all assume 8-bit bytes (§6, §7); the cell model must never leak into compat ABIs. |
| D7 | **Compat is API-shim, never binary-emulation-first**: DOS INT 21h subset → NE → frozen Win32 named-list → Linux static-first | Matches prior art (WSL1, HX extender) and the empirical Wine lesson (§6): per-program pass/fail lists, never global claims. |
| D8 | **Graphics: VGA Mode 13h → VESA/BGA → tiny software GL → (much later) llvmpipe-class / real GPU** | Mode 13h needs no driver (§8); BGA is the virtual-GPU path; llvmpipe is the sane x86_64-future answer, not a from-scratch GL. Vulkan stays research (§8). |
| D9 | **Pixel buffers live in 8-bit regions, always** | No shift/mask in the per-pixel hot path; NetBurst punishes unpredictable branches and cache-line splits (§1, §8). |
| D10 | **Epics never block staged work** (FULL_ROADMAP ordering rules) | The only schedule defense against the Phase 9/10 epics swallowing the project. |

---

## 3. Phased implementation plan (→ FULL_ROADMAP.md)

### Phase 0 — Scaffold & Honest Design — v1-real ✅ (done)
*State*: repo layout, packed cell layer + allocator + `trit.h`
host-tested, architecture/scope/roadmap docs.
*Required reading*: RESEARCH §2, §3.

### Phase 1 — Pentium 4 Bring-up — staged
*Work*: BIOS boot sector → E820 map → protected mode (GDT per
RESEARCH §4 ritual) → bootstrap paging → higher-half `kmain` →
PIC remap → IDT/ISRs → PIT tick → serial + keyboard. QEMU
`qemu-system-i386` with `-cpu qemu32,+sse2` (RESEARCH §1 pass 2:
the installed QEMU has **no `pentium4` model** — pentium3,
Conroe/core2duo, qemu32 are present; NetBurst-class emulation means
qemu32 with SSE2 explicitly enabled). Enable SSE2
(CR4.OSFXSR/OSXMMEXCPT = bits 9/10, confirmed). Write
`docs/PENTIUM4.md` microarchitecture notes (pipeline,
trace cache, Rapid Execution Engine, HT, caches, errata) as the
isolated "rewrite hook".
*Exit*: kernel prints to serial under QEMU P4 emulation; timer IRQ
handled without triple fault.
*Required reading*: RESEARCH §1, §4, §10.

### Phase 2 — Kernel Core Services — staged
*Work*: preemptive scheduler (`T_UNKNOWN` = defer/retry),
message-passing IPC with width-tagged buffers, PCI enumeration,
driver model + IRQ registration. Keep hot paths branch-predictable
(§1: mispredict tax).
*Exit*: two processes ping-pong; one PCI device enumerated;
preemption demo runs.
*Required reading*: RESEARCH §1 (pipeline/HT), §4 (APIC/PIC).

### Phase 3 — Firmware Maturity — staged → epic
*Work (staged)*: Multiboot-compliant loader, GRUB boot, FAT/ext2
second-stage reader. *Work (epic)*: coreboot port for one P4 board
only if hardware is chosen; budget months (§9).
*Required reading*: RESEARCH §9, §4.

### Phase 4 — Toolchain — staged → epic
*Work (staged)*: `i686-elf-gcc` cross toolchain, NASM, linker
scripts, reproducible kernel builds, cell/trit intrinsics as
macros. *Work (epic)*: self-hosting C-subset compiler; the
cell/ternary-*native* type system is Phase 11 research (§3).
*Required reading*: RESEARCH §10, §3.

### Phase 5 — Graphics — v1-real → staged
*Work*: VGA Mode 13h framebuffer + text console + palette +
double buffering (G1–G5 kernel interfaces per GRAPHICS_COMPAT.md);
then VESA/BGA linear fb; then tiny fixed-point software GL
(spinning triangle; validate on host via PPM dumps *before* the
8086/P4 fixed-point port — RESEARCH §8 risk).
*Required reading*: RESEARCH §8, §1 (cache-line discipline).

### Phase 6 — Compatibility I — v1-real → staged
*Work*: `.COM`/MZ `.EXE` loader + INT 21h subset (host-tested
first); real-mode IVT hook or VM86 (§4); INT 10h video shim; NE
loader + 16-bit thunks (Win3.1); Unix V4 API shim + hello-world.
*Required reading*: RESEARCH §5, §6 (NE), §4 (VM86).

### Phase 7 — Shell — staged
*Work*: implement SHELL_SPEC.md — dual grammar, builtin maps,
`.bat`/`.sh` scripting, `$?`/`%ERRORLEVEL%` trit mapping,
Debian-style verbs.
*Required reading*: RESEARCH §7 (for the Linux-ism surface).

### Phase 8 — Compatibility II (Linux/Debian subset) — staged
*Work*: ELF loader + Linux i386 syscall subset (`int 0x80`
convention); static binaries first — musl-static corpus as the
test list (§7); dynamic linker later. Strict 8-bit view (D6).
*Exit*: named list of static binaries runs unmodified.
*Required reading*: RESEARCH §7.

### Phase 9 — Compatibility III (Vista-era Win32) — epic
*Work*: frozen Win32 subset against a *named, tested program
list*; per-program pass/fail docs. Never a global claim (D7, §6).
Runs in parallel once entries met; never blocks other phases.
*Required reading*: RESEARCH §6.

### Phase 10 — Real GPU Driver Program — epic
*Work*: pick one P4-era AGP GPU; PCI/AGP enum → MMIO → command
ring → IRQ → modesetting → 2D accel → one shader compiler.
Expected duration: years (§8). Never blocks anything.
*Required reading*: RESEARCH §8.

### Phase 11 — Research Frontiers — research
Cell/ternary-native compiler, Vulkan-on-packed-regions,
firmware-level cell support, allocator formal verification.
Papers/prototypes, not products.

---

## 4. Risk register

| ID | Risk | Phase | Mitigation |
|----|------|-------|------------|
| R1 | Packed bit-stream hot paths too slow / mispredict-heavy on NetBurst | 1, 2, 5 | 8-bit regions for hot data (D9); host-benchmark before committing; keep padded mode as fallback |
| R2 | QEMU has no `pentium4` CPU model (verified 2026-09-18 in installed QEMU) | 1 | Use `-cpu qemu32,+sse2`; document the delta from NetBurst in `docs/PENTIUM4.md` |
| R3 | Triple-fault debugging burns weeks | 1 | Serial-from-day-one, `-d int -no-reboot`, GDB stub (§10); crude exception handlers first |
| R4 | Allocator fragmentation across mixed widths | 2 | Width-segregated pools if first-fit degrades; fuzz the allocator on host |
| R5 | `T_UNKNOWN` retry livelock in scheduler | 2 | Bounded retries → degrade to error; never unbounded defer |
| R6 | Win32/Vulkan/GPU scope creep re-labels epics as commitments | 9, 10, 11 | Ordering rule D10; per-program lists; labels enforced in review |
| R7 | Second search pass never happens; §2/§3/§5/§6/§7/§9 stay thin | all | Verification backlog in RESEARCH.md; treat flagged claims as provisional |
| R8 | SSE2 enable bits wrong → #UD on first XMM use | 1 | Confirm CR4 bits in SDM Vol. 3 before writing `enable_sse()` |

---

## 5. First 30 days — concrete task list

**Week 1 — Prove the machine boots**
1. Verify QEMU CPU models; document the chosen `-cpu` flag (R2).
2. 512-byte boot sector: print via BIOS, load stage 2, halt. Assemble
   with NASM, boot in QEMU.
3. Stage 2: E820 memory map via INT 15h; A20 enable; GDT (null/code/
   data, flat 4 GB); enter protected mode per the §4 ritual; far jump;
   reload segments; stack; `lidt` with a minimal IDT.
4. Serial port logging working before anything else (§10).

**Week 2 — Protected-mode kernel skeleton**
5. Higher-half bootstrap paging; jump to C `kmain`.
6. PIC remap; IDT with ISRs; crude exception handlers (print-and-halt).
7. PIT at 100–1000 Hz; timer IRQ increments a tick counter.
8. PS/2 keyboard IRQ → scancode → serial echo.
9. `docs/PENTIUM4.md`: NetBurst notes from RESEARCH §1 (the rewrite
   hook — keep P4 assumptions labeled).

**Week 3 — Bring the alien parts into the kernel**
10. Port `cell.h`/`alloc.c`/`trit.h` into kernel address space;
    kernel self-test at boot (round-trips, rewidth).
11. SSE2 enable (confirm bits in SDM — R8); XMM smoke test.
12. Framebuffer: VGA Mode 13h via BIOS *before* leaving real mode
    (or VM86 later); `k_fb_acquire`/`k_fb_present` stubs wired to
    `0xA0000`.
13. Fuzz the allocator on host; fix what breaks (R4).

**Week 4 — First compat + first docs of record**
14. `.COM` loader + INT 21h subset (`09h`, `3Dh/3Eh/3Fh/40h`,
    `4Ch`) running a real `.COM` under QEMU (Phase 6 v1-real).
15. Update `docs/REVIEW.md` with everything that broke (per the
    roadmap's phase rule); run the second web-search verification
    pass for the thin topics (RESEARCH.md backlog).
16. Demo milestone: boot → kernel shell on serial + VGA text →
    `trit`/`alloc` demo commands → exit code. Screenshot it.

**Definition of "month one done"**: QEMU boots our loader into a
32-bit P4 kernel with serial, timer, keyboard, framebuffer text,
self-tested cell/allocator/trit core, and one real DOS `.COM`
executing. Everything else is Phase ≥2.

---

*End of proposal. Disagreements with any decision D1–D10 should be
recorded as ADRs (architecture decision records) under
`docs/adr/` rather than argued in chat.*
