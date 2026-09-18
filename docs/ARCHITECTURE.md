# Architecture — 10-bit on 8086

## 1. The core contradiction

Intel 8086 (1978):
- 8-bit bytes, 16-bit registers, 20-bit segmented addressing (1MB)
- No native 10-bit unit. All memory access is byte-addressed.

A "10-bit architecture" on 8086 therefore cannot be native hardware.
It must be a *virtual* cell size enforced by software:

Option A — padded: store each 10-bit cell in a 16-bit word, waste 6 bits.
  Pro: simple, aligned, fast-ish. Con: 37.5% memory waste.

Option B — packed: pack 10-bit cells back-to-back across words
  (e.g. 8 cells in 5 words). Pro: dense. Con: every load/store needs
  shift+mask, unaligned across word boundaries, slow and complex.

For learning, we start with Option A, then optionally implement B
behind the allocator.

**Update 2026-09-17:** per decision, v1 uses **packed (Option B)** with
discrete widths **8, 10, 13-bit**. See `src/kernel/cell.h` /
`src/kernel/alloc.h`. Rationale: density matters more than speed for the
experiment, and the allocator already hides the shift/mask cost.
8-bit exists for DOS-compat views, 13-bit as an extra non-power-of-two
to stress the packing logic.

This is explicitly *not* performance-efficient — as you requested,
we accept the loss to study the architecture.

## 2. Dynamic bit-width memory assignment

The physical allocator works in 16-bit words but hands out *logical*
regions tagged with width:

```
struct region {
  uint16_t *base;   // physical word address
  uint8_t  width;   // 8, 10, 16, ...
  uint16_t cells;   // count in logical cells
};
```

Provisioning API (sketch):
- `alloc_cells(width, count)` → region
- `free_region(region)`
- `rewidth(region, new_width)` → may move/copy

The kernel tracks fragmentation from mixed widths. This is where the
10-bit experiment lives: user code thinks in 10-bit cells, kernel
translates to 8086 words.

## 3. Binary ↔ ternary boolean at kernel level

We define a kernel-native ternary type:

```
typedef enum { T_FALSE = 0, T_UNKNOWN = 1, T_TRUE = 2 } trit;
```

Stored in 2 bits (wasteful but simple), or packed into 10-bit cells
(5 trits per cell with room to spare).

Kernel rules:
- All kernel decisions can return trit; `T_UNKNOWN` propagates (Kleene logic):
  - NOT: ¬T_UNKNOWN = T_UNKNOWN
  - AND: T_UNKNOWN ∧ T_FALSE = T_FALSE, etc.
- Binary compat: `bool_from_trit()` maps T_UNKNOWN → false (or trap,
  configurable). `trit_from_bool()` is lossless.
- Syscalls that need strict binary get a `strict` flag; otherwise ternary
  flows through.

This is encoded at the lowest level: the syscall return convention
reserves 2 bits for trit, and the scheduler understands "unknown" as
"retry / defer" in some paths.

## 4. Unix Version 4 (1975) compatibility layer

Unix V4 ran on PDP-11, written in C, with ~40 syscalls (open, read,
write, close, fork, exec, wait, etc.). Binary compatibility is impossible
(PDP-11 vs 8086 vs our 10-bit VM). Instead:

- We implement a *source/shim* layer: a small C library that exposes
  V4-like syscall numbers and semantics, translated to our kernel calls.
- Example mapping:
  - V4 `read(fd, buf, n)` → our `k_read(handle, cells, n, width)`
    with width translation and trit-aware error returns
    (0 = ok, 1 = unknown/retry, 2 = error — mapped back to -1/errno for V4 callers)
- Goal is to run *V4-style* programs recompiled against our libc,
  not original V4 binaries.

## 5. Boot on 8086

Real 8086 boot:
- BIOS loads 512-byte boot sector at 0x7C00, real mode, 16-bit.
- Our boot sector (in `src/boot/`) will set up segments, enable a
  minimal stack, load the kernel, then jump to it.
- The 10-bit VM starts *after* this — the bootloader itself is plain
  8086 assembly.

We can test with QEMU (`qemu-system-i386`) emulating 8086 real mode.
No 10-bit hardware is assumed.

## 6. Honest limits

- This will never be a *native* 10-bit CPU — it's an emulated cell model.
- Performance will be poor by design; that's accepted for learning.
- Unix V4 compat is API-level, not binary.
- Ternary logic is a kernel convention, not hardware.

## 7. DOS binary support (Phase 1) + Windows 3.1 provisioning (Phase 2)

8086 real mode already runs 8086 instructions natively, so "native"
here means the *OS services*, not CPU emulation:

- **DOS target:** loader for `.COM` (flat, ORG 0x100) and MZ `.EXE`
  (relocations). An INT 21h shim translates a useful subset
  (09h print string, 3Dh/3Eh/3Fh/40h file IO, 4Ch exit, ...) into our
  `k_*` kernel calls. DOS programs get an 8-bit-wide region view so
  their byte assumptions hold; the kernel's 10/13-bit regions live
  elsewhere. See `src/kernel/dos_compat.h`.
- **Windows 3.1 (later):** NE executables, 16-bit protected mode,
  USER/GDI/KERNEL thunks. v0 only *reserves* the ABI
  (`win31_load_ne`, `win31_thunk_16` return unimplemented). Real
  support is a large follow-up: segment handling, message loop,
  GDI stubs. We provision the headers now so kernel revisions can
  grow without breaking the DOS path.

What "native" does NOT mean: running unmodified Windows binaries
with full GUI fidelity in v1. It means the loader + syscall bridge
exists and is honest about its subset.

Next: pick padded vs packed for the first allocator, and define the
trit syscall ABI precisely before writing C/asm.
