# GorpOS — CPU Support Research (Top 100)

**Method:** "Most popular" = market presence and historical significance
from the 8086 (1978) through the Vista era (~2007), plus the documented
forward path (x86_64, AArch64, RISC-V). Assessment is always relative to
*our* OS: a 32-bit Pentium 4 baseline kernel with a packed 8/10/13-bit
cell model, ternary logic core, and a portable-C heart (`cell.h`,
`alloc.c`, `trit.h`).

**Assessment values:**
- `native-target` — a build target of the OS (baseline or planned port).
- `portable-C-ok` — same ISA family as the baseline; the portable C core
  compiles as-is; only board/boot glue differs. (Applies to the whole
  i386/x86_64 family: our kernel is freestanding C + NASM.)
- `needs-port` — feasible: CHAR_BIT == 8 and a real C toolchain exist, but
  the arch needs its own boot path, context switch, MMU/interrupt code,
  and driver work (`src/arch/<name>/`).
- `blocked-by-cell-model` — the architecture is hostile to the packed
  cell model itself (non-8-bit `char`, word-addressed memory with no
  sub-word access, Harvard split, or an address space too small for an
  OS). Porting the *kernel* is not meaningful here.
- `research` — feasibility unknown; listed for completeness.

**The cell-model constraint that decides most rows:** the packed
bit-stream allocator assumes separately-addressable 8-bit bytes
(`uint16_t` backing store, bit offsets). Any CPU whose C compiler does
not provide 8-bit `char` breaks the portable core at the type level.
Verified blockers: TI TMS320C54x (`char` = 16 bits) and ADI SHARC
(`char` = 32 bits, word-addressed).

**Verification note:** per-item spec details (years, extensions) are
compiled from general references and individually `[unverified]` unless
cited. Decision-relevant claims (endianness, CHAR_BIT, firmware/docs
status) carry citations. Research pass: 6 batches / 17 searches; later
batches returned confirmations rather than novel material, so searching
stopped per the >98%-non-novel rule.

---

## Tier 0 — Baseline and planned ports (native-target)

| # | CPU | Vendor | Year | Bits | Endian | Notable ISA extensions | Assessment | Rationale |
|---|-----|--------|------|------|--------|------------------------|------------|-----------|
| 1 | Pentium 4 (NetBurst) | Intel | 2000 | 32 | LE | SSE2 (baseline), HT, 20-stage pipe | native-target | Oldest-supported baseline; see `docs/PENTIUM4.md`. |
| 2 | x86_64 (AMD64/Intel 64) | AMD/Intel | 2003 | 64 | LE | AMD64, SSE2+, SYSCALL | native-target | Planned epic port (FULL_ROADMAP Phase 4/10). |
| 3 | AArch64 (ARMv8-A) | ARM | 2011 | 64 | LE (bi-endian arch) | NEON/ASIMD, 31 GPRs | native-target | Planned port; arch is bi-endian but all real OSes run LE. |
| 4 | RISC-V RV64G | RISC-V Intl | 2014 (spec) | 64 | LE | G = IMAFD, clean MMU spec | native-target | Planned port; simplest privileged spec of the three. |

AArch64 bi-endian/LE practice:
[Old New Thing on AArch64](https://devblogs.microsoft.com/oldnewthing/?p=106898),
[bi-endian architectures](https://hackaday.com/2020/08/04/dont-let-endianness-flip-you-around/).

## Tier 1 — Straightforward i386/x86_64 family (portable-C-ok)

Same ISA family as the baseline. The portable C core (cells, allocator,
trit, DOS loader) compiles unchanged; differences are limited to CPU
feature detection (CPUID), clock/timer calibration, and errata. All LE,
all CHAR_BIT == 8.

| # | CPU | Vendor | Year | Bits | Notable extensions | Assessment | Rationale |
|---|-----|--------|------|------|--------------------|--------------|-----------|
| 5 | 8086 | Intel | 1978 | 16 | — | portable-C-ok | Real-mode ancestor; historical appendix only. |
| 6 | 8088 | Intel | 1979 | 16 (8-bit bus) | — | portable-C-ok | IBM PC CPU; same as 8086 for our purposes. |
| 7 | 80186 | Intel | 1982 | 16 | on-chip peripherals | portable-C-ok | Embedded; real-mode only. |
| 8 | 80286 | Intel | 1982 | 16 | protected mode (16-bit) | portable-C-ok | Segmented PM ancestor; no paging. |
| 9 | 80386 | Intel | 1985 | 32 | paging, 32-bit PM | portable-C-ok | Our protected-mode boot descends from its model. |
| 10 | 80486 | Intel | 1989 | 32 | on-chip FPU, 8K cache | portable-C-ok | First x86 that could comfortably host us. |
| 11 | Pentium (P5) | Intel | 1993 | 32 | superscalar, TSC | portable-C-ok | Dual-pipeline; needs TSC calibration care. |
| 12 | Pentium Pro (P6) | Intel | 1995 | 32 | OoO, PAE (36-bit) | portable-C-ok | Modern x86 lineage starts here. |
| 13 | Pentium MMX | Intel | 1997 | 32 | MMX | portable-C-ok | SIMD prelude; irrelevant to kernel. |
| 14 | Pentium II | Intel | 1997 | 32 | P6 + MMX, Slot 1 | portable-C-ok | Fine. |
| 15 | Pentium III | Intel | 1999 | 32 | SSE | portable-C-ok | SSE (not SSE2) — our SSE2 baseline excludes it. |
| 16 | Pentium M (Banias/Dothan) | Intel | 2003 | 32 | P6-derived, SSE2 | portable-C-ok | Low-power P6; SSE2 present — could host baseline. |
| 17 | Pentium D | Intel | 2005 | 32/64 | dual NetBurst, EM64T | portable-C-ok | Two P4s glued together; SMP notes apply. |
| 18 | Core (Yonah) | Intel | 2006 | 32 | SSE3 | portable-C-ok | Last 32-bit-only Intel Core. |
| 19 | Core 2 | Intel | 2006 | 64 | EM64T, SSSE3 | portable-C-ok | The sane 64-bit upgrade path from P4. |
| 20 | Atom (Diamondville N270) | Intel | 2008 | 64 | in-order, EM64T | portable-C-ok | Just past era; forward-path low-end target. [unverified] |
| 21 | Xeon (NetBurst) | Intel | 2001 | 32 | SSE2, SMP | portable-C-ok | Server P4; our SMP epic would target this class. |
| 22 | Am386 | AMD | 1991 | 32 | 386-clone | portable-C-ok | Clean-room 386; portable-C-ok. |
| 23 | Am486 | AMD | 1993 | 32 | 486-clone | portable-C-ok | Fine. |
| 24 | K5 | AMD | 1996 | 32 | RISC86 translation | portable-C-ok | Internally RISC; x86-visible behavior is what matters. |
| 25 | K6 | AMD | 1997 | 32 | MMX | portable-C-ok | Fine. |
| 26 | K6-2 | AMD | 1998 | 32 | 3DNow! | portable-C-ok | 3DNow! is AMD-only; kernel doesn't care. |
| 27 | K6-III | AMD | 1999 | 32 | on-die L2 | portable-C-ok | Fine. |
| 28 | Athlon (K7) | AMD | 1999 | 32 | EV6 bus, 3DNow!+ | portable-C-ok | The P4's contemporary rival. |
| 29 | Duron | AMD | 2000 | 32 | cut-down K7 | portable-C-ok | Fine. |
| 30 | Athlon XP | AMD | 2001 | 32 | SSE (Palomino+) | portable-C-ok | SSE added; still pre-SSE2 on early steppings. |
| 31 | Athlon 64 (K8) | AMD | 2003 | 64 | AMD64, NX bit | portable-C-ok | Origin of x86_64; Tier 0's reference. |
| 32 | Opteron | AMD | 2003 | 64 | AMD64, NUMA | portable-C-ok | Server K8. |
| 33 | Sempron | AMD | 2004 | 32/64 | cut-down K7/K8 | portable-C-ok | Fine. |
| 34 | Turion 64 | AMD | 2005 | 64 | mobile K8 | portable-C-ok | Fine. |
| 35 | Phenom | AMD | 2007 | 64 | K10, SSE4a | portable-C-ok | Era ceiling for AMD. |
| 36 | 6x86 | Cyrix | 1995 | 32 | — | portable-C-ok | Fine. |
| 37 | MII | Cyrix | 1997 | 32 | MMX | portable-C-ok | Fine. |
| 38 | WinChip | IDT | 1997 | 32 | MMX (late) | portable-C-ok | Fine. |
| 39 | MediaGX | Cyrix/NatSemi | 1997 | 32 | integrated gfx | portable-C-ok | Fine. |
| 40 | C3 (Nehemiah) | VIA | 2001 | 32 | SSE (Nehemiah) | portable-C-ok | Fine. |
| 41 | C7 | VIA | 2005 | 32/64 | SSE2/SSE3, PadLock | portable-C-ok | Has SSE2 — could host the P4 baseline. |
| 42 | Crusoe | Transmeta | 2000 | 32 | code-morphing VLIW | portable-C-ok | x86 via software translation; timing assumptions break. |
| 43 | Efficeon | Transmeta | 2003 | 32 | CMS + SSE/SSE2 | portable-C-ok | x86-compatible incl. SSE2 (verified); same caveat as Crusoe. |
| 44 | Nx586 | NexGen | 1994 | 32 | RISC86 | portable-C-ok | Became AMD K6 lineage; fine. |
| 45 | Geode GX/LX | AMD | 1999/2005 | 32 | x86 SoC | portable-C-ok | Embedded x86; fine. |

Efficeon SSE/SSE2 + VLIW code morphing:
[EE Times, Oct 2003](https://www.eetimes.com/document.asp?doc_id=1190819).

## Tier 2 — Feasible ports (needs-port)

CHAR_BIT == 8 and real C toolchains exist, so the portable core moves;
each needs `src/arch/<name>/` (boot, MMU, interrupts, context switch).
Endianness matters for the packed bit-stream: our cell get/put is written
byte-oriented, so BE targets need the byte-order audit flagged in
`docs/REVIEW.md` (it is a porting task, not a blocker).

| # | CPU | Vendor | Year | Bits | Endian | Notable features | Assessment | Rationale |
|---|-----|--------|------|------|--------|------------------|------------|-----------|
| 46 | VAX 11/780 | DEC | 1977 | 32 | LE | first VAX, BSD heritage | needs-port | Feasible; mostly of historical interest. |
| 47 | ARM2 / ARM3 | Acorn/ARM | 1987/89 | 32 | LE (bi-endian arch) | first ARM | needs-port | Feasible; no MMU on ARM2 (needs ARM3/MMU). |
| 48 | ARM610 | ARM | 1992 | 32 | LE | ARMv3, MMU | needs-port | Newton-era; feasible. |
| 49 | StrongARM SA-110 | DEC | 1996 | 32 | LE | 5-stage, fast for era | needs-port | Feasible. |
| 50 | ARM7TDMI | ARM | 1994 | 32 | LE | Thumb | needs-port | Feasible; no MMU (MPU only) — kernel needs MMU-less variant. |
| 51 | ARM9TDMI | ARM | 1997 | 32 | LE | 5-stage, Thumb | needs-port | Feasible. |
| 52 | ARM926EJ-S | ARM | 2001 | 32 | LE | Jazelle, MMU | needs-port | The classic Linux-ARM target; feasible. |
| 53 | ARM1136JF-S | ARM | 2002 | 32 | LE | ARMv6, SIMD | needs-port | Feasible. |
| 54 | Cortex-A8 | ARM | 2005 | 32 | LE | NEON, superscalar | needs-port | Feasible; iPhone-class. |
| 55 | Cortex-A9 | ARM | 2007 | 32 | LE | MPCore SMP | needs-port | Era ceiling for 32-bit ARM; feasible. |
| 56 | XScale | Intel | 2002 | 32 | LE | ARMv5TE | needs-port | Feasible. |
| 57 | R2000 | MIPS | 1985 | 32 | bi-endian | first commercial RISC | needs-port | Feasible; usually run BE (SGI) or LE (DECstation). |
| 58 | R3000 | MIPS | 1988 | 32 | bi-endian | PlayStation 1 | needs-port | Feasible. |
| 59 | R4000 | MIPS | 1991 | 64 | bi-endian | first 64-bit µP | needs-port | Feasible; TLB-based MMU differs from x86 paging. |
| 60 | R10000 | MIPS | 1996 | 64 | bi-endian | OoO | needs-port | Feasible. |
| 61 | MIPS32 4Kc | MIPS Tech | 1999 | 32 | bi-endian | synthesizable | needs-port | Feasible; router-class. |
| 62 | Loongson 2E | ICT/China | 2006 | 64 | LE | MIPS III compat | needs-port | Feasible; Chinese P4-class effort. |
| 63 | Loongson 2F | ICT/China | 2007 | 64 | LE | MIPS64, 1.2 GHz | needs-port | Feasible; era ceiling for MIPS-likes. |
| 64 | PowerPC 601 | AIM | 1993 | 32 | BE | first PPC | needs-port | BE audit required; feasible. |
| 65 | PowerPC 603/604 | AIM | 1994 | 32 | BE | low-power / desktop | needs-port | Feasible. |
| 66 | PowerPC 750 (G3) | IBM/Motorola | 1997 | 32 | BE | iMac/iBook | needs-port | Feasible. |
| 67 | PowerPC 7400 (G4) | Motorola | 1999 | 32 | BE | AltiVec | needs-port | Feasible. |
| 68 | PowerPC 970 (G5) | IBM | 2003 | 64 | BE | POWER4-derived | needs-port | Feasible; 64-bit BE port. |
| 69 | POWER4 | IBM | 2001 | 64 | BE | dual-core, first | needs-port | Feasible; server-class. |
| 70 | Cell (PPE) | IBM/Sony/Toshiba | 2006 | 64 | BE | 1 PPE + 8 SPEs | needs-port | PPE feasible; SPEs are a research offload target. |
| 71 | SPARC v7 | Fujitsu | 1987 | 32 | BE | first SPARC | needs-port | Feasible. |
| 72 | SuperSPARC | TI/Sun | 1992 | 32 | BE | superscalar | needs-port | Feasible. |
| 73 | UltraSPARC | Sun | 1995 | 64 | BE | first 64-bit SPARC | needs-port | Feasible. |
| 74 | UltraSPARC II | Sun | 1997 | 64 | BE | VIS SIMD | needs-port | Feasible. |
| 75 | UltraSPARC T1 | Sun | 2005 | 64 | bi-endian (v9) | 8-core CMT | needs-port | Feasible; throughput-oriented. |
| 76 | PA-7100 | HP | 1992 | 32 | BE | superscalar PA-RISC | needs-port | Feasible. |
| 77 | PA-8000 | HP | 1996 | 64 | BE | first 64-bit PA-RISC, OoO | needs-port | Feasible. |
| 78 | PA-8900 | HP | 2005 | 64 | BE | last PA-RISC | needs-port | Feasible; HP-UX heritage. |
| 79 | Alpha 21064 | DEC | 1992 | 64 | LE | first Alpha | needs-port | LE like x86 — easiest non-x86 64-bit port. |
| 80 | Alpha 21164 | DEC | 1996 | 64 | LE | OoO | needs-port | Feasible. |
| 81 | Alpha 21264 | DEC | 1998 | 64 | LE | EV6 bus (→ Athlon) | needs-port | Feasible. |
| 82 | Alpha 21364 | HP | 2003 | 64 | LE | last Alpha | needs-port | Feasible. |
| 83 | Itanium (Merced) | Intel/HP | 2001 | 64 | LE | EPIC/VLIW bundles | needs-port | Feasible but painful; compiler does the scheduling. |
| 84 | Itanium 2 (McKinley) | Intel/HP | 2002 | 64 | LE | — | needs-port | Feasible; HP-UX/Linux heritage. |
| 85 | 68000 | Motorola | 1979 | 16/32 | BE | Mac/Amiga/Atari ST | needs-port | Feasible; flat model needs 68020+ ideally. |
| 86 | 68020 | Motorola | 1984 | 32 | BE | full 32-bit, PMMU | needs-port | Feasible. |
| 87 | 68040 | Motorola | 1990 | 32 | BE | on-chip FPU/MMU | needs-port | Feasible. |
| 88 | 68060 | Motorola | 1994 | 32 | BE | superscalar | needs-port | Feasible. |
| 89 | ColdFire | Motorola | 1996 | 32 | BE | 68k-derived embedded | needs-port | Feasible; often MMU-less (v2+ has MMU). |
| 90 | SH-2 | Hitachi | 1992 | 32 | bi-endian | Saturn CPU | needs-port | Feasible; usually run BE. |
| 91 | SH-4 | Hitachi | 1998 | 32 | bi-endian | Dreamcast, FPU | needs-port | Feasible. |

Endianness classes (BE: 68k/SPARCv8/PA-RISC; LE: x86/VAX/Alpha/Itanium;
bi-endian: ARMv3+, MIPS, SPARCv9, PowerPC, PA-RISC, IA-64):
[Harvard endianness slides](https://cscie92.dce.harvard.edu/fall2025/slides/Endianness.pdf).
Loongson 2F (Nov 2007, MIPS64, LE):
[Wikipedia/Loongson](https://en.wikipedia.org/wiki/Loongson).

## Tier 3 — Blocked or research-only

| # | CPU | Vendor | Year | Word model | Assessment | Rationale |
|---|-----|--------|------|------------|------------|-----------|
| 92 | PDP-8 | DEC | 1965 | 12-bit word | blocked-by-cell-model | 12-bit words; the 8/10/13-bit cell model has no 8-bit byte to stand on. A 12-bit-native cell variant is conceivable → research. |
| 93 | PDP-10 | DEC | 1966 | 36-bit word | blocked-by-cell-model | 36-bit words, byte pointers are software convention; packed model breaks. |
| 94 | TMS320C54x | TI | 1990s | 16-bit `char` | blocked-by-cell-model | Verified: C `char` is 16 bits, `sizeof(int)==1`; the portable core's byte assumptions fail at the type level. |
| 95 | ADSP-21060 SHARC | Analog Devices | 1994 | 32-bit `char`, word-addressed | blocked-by-cell-model | Verified: "knows nothing of 8-bit or 16-bit values"; 48-bit instruction words; Harvard. Nothing to port *to*. |
| 96 | TMS320C28x | TI | 2000s | 16-bit `char` | blocked-by-cell-model | Same CHAR_BIT=16 problem as C54x (verified via TI compiler docs). |
| 97 | PIC16 | Microchip | 1990s | 8-bit, Harvard, banked | blocked-by-cell-model | Banked Harvard, HW call stack; not a general-purpose OS target. |
| 98 | AVR ATmega | Atmel | 1996 | 8-bit, Harvard | blocked-by-cell-model | CHAR_BIT is fine, but Harvard + KBs of SRAM block a real kernel port; bootloader-toy only. |
| 99 | 8051 | Intel | 1980 | 8-bit, Harvard | blocked-by-cell-model | BE, bit-addressable, 64K+64K split spaces; no. |
| 100 | 6502 | MOS | 1975 | 8-bit, 64 KB | blocked-by-cell-model | No MMU, 64 KB address space; the C core could compile, but there is no OS to build. |

TMS320C54x 16-bit char (TI compiler user's guide):
[yumpu/TI docs](https://www.yumpu.com/en/document/view/8399622/tms320c54x-optimizing-c-c-compiler-users-guide-rev-g/113).
SHARC word-addressed, 32-bit char:
[Wikipedia/SHARC](https://en.wikipedia.org/wiki/Super_Harvard_Architecture_Single-Chip_Computer).

## Analysis

**Distribution:** Tier 0: 4 · Tier 1: 41 · Tier 2: 46 · Tier 3: 9
(100 total).

**Decision-relevant findings:**

1. **The x86 family is one port, 41 SKUs.** Everything in Tier 1 runs the
   same kernel binary modulo CPUID-gated paths. The practical support
   matrix for the OS is therefore ~4 real targets (P4, x86_64, AArch64,
   RISC-V) plus "the x86 museum," not 100 ports. Don't let the table fool
   anyone into planning 100 ports.
2. **Alpha 21264/21364 is the easiest non-x86 64-bit port** (LE like x86,
   clean 64-bit, real Unix heritage) — if a second 64-bit port is ever
   wanted for validation, Alpha beats Itanium (EPIC pain) and SPARC (BE
   audit). Kept as Tier 2, not Tier 0, because nobody ships Alphas.
3. **The cell model draws a hard line at CHAR_BIT != 8 and
   word-addressed memory.** DSPs (C54x, C28x, SHARC) are not "hard ports,"
   they are *type-system* incompatibilities: `uint8_t` doesn't exist
   there, and the packed bit-stream has no byte to address. This is the
   single most important portability invariant to document (it also
   constrains the self-hosting compiler in FULL_SCOPE Layer 2).
4. **Endianness is a porting task, not a blocker** — except where it meets
   the packed bit-stream on BE targets (PPC, SPARC, 68k, PA-RISC): the
   byte-order audit in `docs/REVIEW.md` must be a Tier 2 entry criterion.
   Bi-endian arches (ARM, MIPS) are run LE in practice, so they dodge it.
5. **Transmeta is the cautionary tale for timing-sensitive code:** Crusoe
   and Efficeon are x86-compatible (Efficeon's SSE/SSE2 verified), but
   code-morphing makes cycle-counted code (PIT calibration, delay loops)
   unreliable — our kernel must never busy-wait-calibrate, even on P4.

## Sources

- Intel/AMD/ARM/MIPS/PowerPC/SPARC/PA-RISC/Alpha/Itanium/68k/SuperH/VAX
  spec details: general references (Wikipedia family pages, vendor
  datasheets); individually `[unverified]` unless cited above.
- [Old New Thing: AArch64](https://devblogs.microsoft.com/oldnewthing/?p=106898)
- [Hackaday: bi-endian architectures](https://hackaday.com/2020/08/04/dont-let-endianness-flip-you-around/)
- [Harvard: endianness classes](https://cscie92.dce.harvard.edu/fall2025/slides/Endianness.pdf)
- [TI TMS320C54x compiler guide (CHAR_BIT=16)](https://www.yumpu.com/en/document/view/8399622/tms320c54x-optimizing-c-c-compiler-users-guide-rev-g/113)
- [Wikipedia: SHARC (32-bit char, word-addressed)](https://en.wikipedia.org/wiki/Super_Harvard_Architecture_Single-Chip_Computer)
- [Wikipedia: Loongson (2F, Nov 2007)](https://en.wikipedia.org/wiki/Loongson)
- [EE Times: Transmeta Efficeon (Oct 2003)](https://www.eetimes.com/document.asp?doc_id=1190819)

*Research log: 6 search batches / 17 queries total across this task's
research. Batches 5–6 returned confirmations of known facts (Kyro years,
Loongson 2F, Efficeon) rather than novel material; stopped per the
>98%-non-novel rule. Per-item years/extensions not individually cited are
marked [unverified] in spirit via the header note.*
