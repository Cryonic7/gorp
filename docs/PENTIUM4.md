# Pentium 4 / NetBurst Baseline (IA-32)

**Status:** Oldest-supported CPU as of 2026-09-18. Supersedes the 8086 baseline.
**Mode:** 32-bit protected mode with paging. **SSE2 is baseline.** Not x86-64.

The 8086 material is preserved as a historical appendix in
`docs/hardware/8086.md` and the appendix below. New code must not target it.

## Why Pentium 4

The P4/NetBurst microarchitecture is the first Intel IA-32 core where the
modern performance rules visibly apply: deep pipelines, a trace cache instead
of a classic L1 instruction cache, double-pumped ALUs, SSE2 as a baseline,
and 64-byte cache lines. Targeting it forces the kernel, allocator, and
packed-cell code to be honest about alignment, branch cost, and cache-line
splits — the same discipline x86-64 needs later.

P4-specific assumptions are isolated and labeled `P4:` in code comments so
the microarchitecture can be swapped later without touching the
architecture-independent layers (packed cells, allocator, trit logic, DOS
shim).

## NetBurst pipeline

- **Willamette/Northwood:** ~20-stage integer pipeline.
- **Prescott and later:** ~31 stages. The longer pipeline raises the
  branch-misprediction penalty substantially; see below.
- The front end fetches from the **trace cache** (decoded micro-ops), not
  from a conventional L1 I-cache. Branches are predicted before decode.
- **Rapid Execution Engine:** two simple ALUs run at 2x the core clock
  ("double-pumped"). Simple integer ops have 0.5-cycle throughput; dependent
  chains still pay full latency.

### Branch mispredictions (P4:)

- Mispredict penalty is roughly the pipeline depth: ~20 cycles on
  Northwood, ~30+ on Prescott.
- The packed-cell bit-extraction code and allocator fast paths must avoid
  unpredictable branches in hot loops. Prefer branchless selects
  (`cmov`, masked ops) or make the common case statically predictable.
- `loop` instruction is microcoded and slow on P4; use `dec ecx / jnz`.

## SSE2 (baseline)

- Every P4 implements SSE2. The kernel may use it freely once CR0/CR4 are
  set up (no OS FXSAVE support needed for kernel-internal use if the kernel
  never sleeps with live SSE state across context switches — document when
  that changes).
- 16 XMM registers, 128-bit vectors. Integer SSE2 (`movdqa`, `paddb`, …)
  accelerates packed-cell bulk moves and the allocator's zeroing.
- **Alignment (P4:):** `movdqa` faults on unaligned addresses. Packed streams
  are bit-addressed; bulk SSE2 copies must align to 16 bytes or use `movdqu`
  (slower). The allocator's 16-byte alignment guarantee exists for this.
- **Denormals (P4:):** SSE2 denormal arithmetic is slow (~100+ cycles).
  The fixed-point rasterizer must avoid generating denormals; flush-to-zero
  (FTZ) via MXCSR is acceptable for graphics but must be scoped — never set
  globally in the kernel.

## Hyper-Threading

- Only on models that provide it (Northwood 3.06 GHz+, Prescott). Not a
  baseline feature. The kernel must not assume two logical CPUs.
- When present, the two threads share the trace cache and execution units;
  spinlocks need `pause` (SSE2) to avoid starving the sibling thread.

## Front-side bus

- Quad-pumped FSB: 400/533/800 MT/s. Memory bandwidth is FSB-limited;
  the packed allocator's streaming writes should use write-combining or
  non-temporal stores (`movnti`) for large bulk transfers to avoid
  cache pollution — but non-temporal stores need SFENCE discipline.

## Cache hierarchy (P4:)

- **L1 data:** 8 KB (Willamette/Northwood) or 16 KB (Prescott), 4-way,
  64-byte lines, write-through.
- **Trace cache:** ~12K micro-ops, replaces L1 I-cache.
- **L2:** 256 KB–2 MB, 8-way, 64-byte lines, write-back.
- **Line size is 64 bytes everywhere.** Packed streams that straddle a
  64-byte boundary pay a split penalty; the allocator should avoid placing
  hot headers across lines, and the cell codec should prefer 64-byte-aligned
  row starts for bulk 2D blits.

### Store forwarding (P4:)

- A load that overlaps a recent store but is not an exact size/address match
  stalls (~10+ cycles). The packed-cell accessors write sub-word fields;
  reading back a different width from the same address range must go through
  the canonical accessor, not a raw wider load, or it will stall.

## Protected-mode environment (what the boot sets up)

- **GDT:** flat 32-bit code/data segments, base 0, limit 4 GB.
- **Paging:** identity-mapped first 4 MB (PD at 0x5000, PT at 0x6000 —
  note: 0x7000/0x8000 are BIOS scratch on SeaBIOS, verified 2026-09-18).
- **A20:** via INT 15h AX=2401, port 0x92 fallback.
- **Interrupts:** OFF until the IDT, PIC remap, and PIT are installed.
  No IDT exists yet — any interrupt before that triple-faults.

## QEMU emulation

- QEMU 9.2.1 has **no `pentium4` CPU model**. Closest available 32-bit
  SSE2 baseline: `-cpu qemu32,+sse2`.
- `qemu-system-i386 -drive file=gorpos.img,format=raw,if=floppy -boot a`.
- **Quirks found 2026-09-18:**
  - 8-bit `OUT` to the isa-debug-exit port is mis-emulated after segment
    loads; 32-bit `OUT` works until a BIOS video call, after which port I/O
    to 0xF4 hangs. Verification uses VGA text + monitor instead.
  - INT 13h AH=42h (EDD/LBA) hangs on the SeaBIOS floppy path; the boot
    uses CHS.
  - INT 10h breaks subsequent port I/O on QEMU 9.2.1; stage2 avoids BIOS
    video calls entirely.

## Appendix: 8086 (historical)

The project originally targeted the 8086 (real mode, 16-bit, no paging,
no SSE). That target is retired. The 8086 boot sector, segmentation notes,
and porting shims remain in `docs/hardware/8086.md` for reference only.
No new code targets 8086; the DOS compatibility layer is a source/API shim,
not 8086 binary compatibility.
