# Adversarial Review — GorpOS v0.1

*Reviewer stance: assume every claim is marketing until the code or a
test proves it. Severity: **C**ritical (wrong behavior / insecurity),
**M**ajor (likely bug, missing check, design debt), **m**inor (polish).*

*Two findings below were bugs caught during this review and fixed
before release: R-1 (fixed), R-2 (fixed). They stay listed so the
pattern isn't repeated.*

## Findings

### R-1 [C — FIXED] `.COM` size check allowed cell-count truncation
`load_com` rejected `len > 64K - 0x100`, then cast `0x100 + len` to
`uint16_t`. At exactly `len = 0xFF00`, cells = `0x10000` → truncated
to `0`, and `alloc_cells(…, 0)` fails — a *valid maximum-size* DOS
program was rejected with "needs more memory." Fix: compute cells in
`uint32_t` and reject `cells > 0xFFFF` before the cast. Regression
test added (`max-size com loads`).

### R-2 [M — FIXED] Corrupt MZ relocation table leaked its region
`load_mz` allocated the 8-bit region, then returned `T_FALSE` on a
bad relocation entry without freeing it — one corrupt binary =
one permanently lost region (up to 64 and the allocator is dead).
Fix: `alloc_free` + `region_id = -1` on that path. Regression test
added (`no region leaked on bad reloc`).

### R-3 [C] No bounds check on `region_get/put` index in C
`alloc.c` trusts `idx < cells`. A kernel caller bug (or a hostile
computation feeding the allocator) reads/writes outside the pool —
silent memory corruption, no trap. The Rust mirror returns
`Option`/`bool` here, so the two implementations have *different*
failure semantics for the same bug. Fix: `assert(idx < r->cells)` in
debug builds at minimum; document that release trusts the caller.

### R-4 [C] No privilege separation — by design, but say it louder
Everything (kernel, allocator, loaded DOS programs) runs at the same
real-mode privilege. A `.COM` can overwrite the kernel's pool, the
IVT, or the allocator metadata. This is *exactly* real DOS, so it's
not a bug — but any future claim like "sandboxed DOS programs" would
be false until there's an 8086 memory-protection story (there isn't
one; the 8086 has no MMU).

### R-5 [M] Duplicated bit-twiddling: `cell.h` vs `alloc.c`
`cell_get_packed`/`cell_put_packed` exist in `cell.h`, but
`region_get`/`region_put` reimplement the same math inline (because of
the `bit_base` offset). Two copies of tricky shift/mask code *will*
diverge. Fix: factor a `pool_get_abs(pool, abs_bit, width)` /
`pool_put_abs(...)` helper in `cell.h` and use it from both places.

### R-6 [M] `find_gap` is O(n²) first-fit with no free list
Fine for ≤64 regions, but fragmentation behavior is untested and
there's no defragmentation story. Mixed 8/10/13-bit workloads will
fragment the bit-space; first-fit from bit 0 also *systematically*
reuses low addresses, which is fine for determinism but worth a test
that hammers alloc/free patterns. The old dead `next_start` variable
was removed during this review.

### R-7 [M] No in-tree test suite
All C tests live in `/tmp` for this session. There is no `tests/`
directory, no Makefile target, nothing a second person can run.
The Rust side is fine (`cargo test`). Fix: commit `test_alloc.c`,
`test_dos.c`, and a `Makefile` with `make test`.

### R-8 [M] MZ loader ignores `min_extra`/`max_extra` and has no stack
A real EXE's BSS and stack (`SS:SP` from the header) are not set up;
we don't even reserve `min_extra` paragraphs. Any EXE that touches
uninitialized data or pushes anything will misbehave. This is
documented in INTERNALS.md but easy to miss — the loader should at
least *warn* (return `T_UNKNOWN`) when `min_extra > 0`.

### R-9 [M — PARTIALLY FIXED] Boot sector is hand-verified only
`boot.asm`/`stage2.asm` now assemble (NASM) and boot in QEMU 9.2.1
(`qemu-system-i386 -cpu qemu32,+sse2`). Verified 2026-09-18 via
VGA+monitor: stage2 reaches 32-bit protected mode, enables paging,
prints "P4". **Remaining:** 73-sector kernel load has issues with
high LBAs (66-sector load works; kernel partially loads). IDT/PIC/PIT
not yet implemented. See docs/PENTIUM4.md for QEMU quirks.

### R-15 [M] P4 branch-misprediction penalty on packed-cell hot paths
NetBurst's ~20-31 stage pipeline makes unpredictable branches in
`cell_get/put` and the allocator's first-fit loop expensive (~20-30+
cycles per mispredict). The current C code uses straightforward
branches. Fix: audit hot loops for branchless alternatives (`cmov`,
predicated ops) once profiling is possible; keep the simple code until
then but do not add new data-dependent branches in the codec.

### R-16 [M] Packed-stream cache-line splits (P4: 64-byte lines)
Bit-packed 8/10/13-bit streams routinely straddle 64-byte cache lines.
A split load/store pays ~10+ cycles on P4. The allocator does not
align region starts to cache lines, and the cell codec does not avoid
splits. Fix: align hot region bases to 64 bytes where cheap; document
that bulk 2D blits should start rows on line boundaries.

### R-17 [M] SSE2 alignment and denormal hazards (P4:)
`movdqa` faults on unaligned addresses; the 16-byte alignment guarantee
in `alloc_cells_aligned` exists for this but is not enforced by the
codec. SSE2 denormal arithmetic is ~100+ cycles on P4; the
fixed-point rasterizer must avoid denormals (scoped FTZ via MXCSR,
never global). No SSE2 is used in the kernel yet — these are
forward-looking constraints.

### R-18 [C] No IDT before enabling interrupts (P4 protected mode)
The boot enters protected mode with interrupts disabled, which is
correct, but there is no IDT, no PIC remap, and no PIT init yet. Any
`sti` before those are installed triple-faults. This is the next
bring-up milestone; do not enable interrupts until R-18 is closed.

### R-19 [M] BSS not cleared on real hardware
`kmain` relies on the loader zeroing BSS. QEMU/SeaBIOS happens to
provide zeroed RAM, but real firmware does not guarantee it. Fix:
add an explicit BSS-clear loop in the kernel entry (linker provides
`__bss_start`/`__bss_end`).

### R-20 [M] Boot-memory-map overlap risk (P4:)
The page directory (0x5000) and page table (0x6000) were moved from
0x7000/0x8000 after discovering SeaBIOS uses 0x7000-0x8FFF as scratch
(verified 2026-09-18: writes to 0x7000 did not stick). The current
map (PD 0x5000, PT 0x6000, stack 0x9000, stage2 0x7E00, kernel 0x10000)
is verified working, but any future change to the map must re-verify
against SeaBIOS's actual usage.

### R-21 [m] Linker produces RWX segment
`kernel.elf` has a LOAD segment with RWX permissions (GNU ld default
for the custom linker script). On real hardware this defeats any
W^X policy. Fix: split text/rodata/data into separate segments with
correct permissions when the memory manager exists.

### R-22 [m] QEMU debug-exit port unreliable (P4:)
The isa-debug-exit device (port 0xF4) hangs after BIOS video calls on
QEMU 9.2.1, and 8-bit OUT is mis-emulated. The project no longer
relies on it for verification (uses VGA+monitor instead). The port
write remains in code as a harmless no-op on real hardware, but do
not gate tests on its exit code.

### R-10 [m] `dos_linear` 20-bit wraparound can alias
`DS:DX` wraps at 1MB like real hardware, but our region starts at
linear 0 — a program passing `DS=0xFFFF, DX=0x0010` lands back inside
its own region. Faithful to 8086, but surprising if you expected a
fault. Documented behavior; leave as-is.

### R-11 [m] Trit value `3` is never validated
Casts from wider ints can smuggle `3` into `trit_t`; the logic
functions fall through to `Unknown`. Harmless today, but a `trit_valid()`
check at trust boundaries would be cheap.

### R-12 [m] INT 21h `09h` 4096-char cap is arbitrary
Fine, but the cap should be a named constant, and `T_FALSE` on a
missing `$` terminator is the right call (prevents overrun) — keep.

### R-13 [m] ARCHITECTURE.md tells the story out of order
It still opens with "we start with Option A (padded)" and only later
notes the switch to packed. A new reader has to hold two designs.
Fix: rewrite §1 in present tense (packed), move padded to a footnote.

### R-14 [m] Rust/C semantic drift risk
`Allocator::get` returns `Option`, C returns garbage-on-bug (R-3);
Rust `put` returns `bool`, C is void. The *happy paths* are
behavior-identical (verified by parallel tests), but the mirrors need
a shared conformance test (same vectors, both languages) to stay that
way. The `spill_over_word_boundary` and `roundtrip_all_widths` tests
are a good start.

## Honesty audit (claims vs reality)

| Claim | Verdict |
|-------|---------|
| "10-bit architecture on 8086" | TRUE as *emulated cell model*; would be FALSE if read as hardware. Docs consistently say emulated. |
| "DOS binary support" | TRUE for `.COM` + MZ load and 4 INT 21h functions; FALSE for anything needing files, PSP, or memory management. Stubs honestly return `T_UNKNOWN`. |
| "Windows 3.1 support" | Explicitly stubs-only (Phase 2). The header says so; keep it that way in every doc. |
| "Hardware support suite" | TRUE as *notes + init sequences* for 8086; porting doc is honest that x86_64/AArch64/RISC-V are unimplemented. |
| "Boots" | PARTIAL — stage2 reaches PM+paging+"P4" in QEMU (VGA+monitor verified 2026-09-18); full kernel handoff pending 73-sector load fix. See R-9. |
| "Rust mirror" | TRUE for cell/alloc/trit; dos_compat and boot are C/asm-only, documented in `rust/README.md`. |
| "Pentium 4 baseline" | TRUE as of 2026-09-18: 32-bit PM, paging, SSE2 baseline. QEMU uses `qemu32,+sse2` (no P4 model in QEMU 9+). See docs/PENTIUM4.md. |

## Prioritized fix list

1. R-3 — bounds-check (or assert) region indices in C.
2. R-18 — IDT + PIC remap + PIT init before any `sti`.
3. R-7 — commit tests + Makefile (`make test`).
4. R-5 — deduplicate bit access into one helper.
5. R-9 — fix 73-sector kernel load; verify full kernel handoff.
6. R-19 — explicit BSS clearing in kernel entry.
7. R-8 — handle/decline EXEs with `min_extra > 0`.
8. R-15/R-16 — audit packed-cell hot paths for branches and cache-line splits (P4).
9. R-6 — fragmentation stress test.
10. R-13 — rewrite ARCHITECTURE.md §1 in present tense.
11. R-14 — shared C/Rust conformance vectors.
12. R-11 — `trit_valid()` at trust boundaries.
13. R-4 — no action (by design); keep the warning prominent.
14. R-17/R-20/R-21/R-22 — P4-specific: SSE2 discipline, memory-map verification, RWX segments, debug-exit unreliability.
