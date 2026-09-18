# Internals — detailed design

Companion to `ARCHITECTURE.md`. This doc pins down the exact algorithms
and ABIs. If code and this doc disagree, the code wins and the doc must
be fixed.

## 1. Packed cell layout (cell.h / rust cell.rs)

Physical store: `uint16_t pool[]`. Logical stream: cells of `width`
bits (8, 10, 13) packed back-to-back, little-endian within the stream.

For cell `idx` at bit offset `b = idx * width`:
- word index `wi = b / 16`, bit offset `bo = b % 16`
- low part: `pool[wi] >> bo`, masked to `width` bits
- if `bo + width > 16`, high part: `pool[wi+1] << (16 - bo)` ORed in

A cell spans at most 2 words (widths ≤ 13 < 16 + 15). Write path:
- `low_mask = (width_mask << bo)` truncated to 16 bits — truncation is
  *correct* here: it selects exactly bits `[bo, 16)` of the low word.
- high bits `rem = bo + width - 16` go to `pool[wi+1]` low bits.

**No-overrun argument:** the last cell's bits end at `count*width`
bits total; `cells_words_needed = ceil(count*width / 16)` words cover
exactly that many bits, and a spill past the final word would require
`bo + width > 16` with `wi + 1 == words`, which implies
`(count-1)*width % 16 + width > 16` while `count*width % 16 == 0` —
impossible (proof: then `bo = 16 - width%16`, `bo + width = 16`).
Still, callers must size pools with `cells_words_needed`.

## 2. Allocator (alloc.c / rust alloc.rs)

State: flat bit-space `[0, pool_words*16)`, up to 64 region descriptors
`{used, width, cells, bit_base, bit_len}`.

- `alloc_cells(w, n)`: validate width ∈ {8,10,13}, `n > 0`; first-fit
  scan for `n*w` free bits; record descriptor. O(regions²), fine for v0.
- `alloc_cells_aligned(w, n, a)`: same, but candidate `bit_base`
  rounded up to a multiple of `a` bits. Used by the DOS loader
  (`a = 8`) so DOS linear addresses map 1:1 to cell indices.
- `alloc_free(id)`: marks descriptor free. No explicit coalescing is
  needed — the scan sees freed intervals as gaps automatically.
- `rewidth(id, w')`: allocates a new region, copies each cell masked to
  `min(old_w, w')` bits (narrowing truncates, widening zero-extends),
  frees the old region. Not atomic across the copy, but failure
  (`None`/`-1`) leaves the old region untouched.
- `region_get/put`: absolute bit = `bit_base + idx*width`, then the
  packed accessors. No bounds check on `idx` in C beyond the
  descriptor's `cells` (Rust returns `Option`/`bool`).

**Fragmentation:** mixed widths fragment the bit-space; first-fit tends
to pack low. Worst case is pathological but acceptable for a learning
kernel; a real kernel would add best-fit or slab classes per width.

## 3. Ternary ABI (trit.h / rust trit.rs)

Encoding: 2 bits, `0 = false, 1 = unknown, 2 = true`, `3` reserved
(never produced; never validated on input — see REVIEW.md).

Kleene logic:
- `not`: flips true/false, unknown stays unknown.
- `and`: false annihilates; true∧true = true; else unknown.
- `or`: dual of and.

Conversions: `to_bool` maps unknown → false (configurable trap in a
stricter kernel via `STRICT_TERNARY`, not yet implemented).
`from_bool` is lossless.

Kernel convention: syscalls return `trit_t`; `T_UNKNOWN` means
"indeterminate — caller may retry or defer" (used by the scheduler
design, not yet wired to a scheduler).

## 4. DOS loader flow (dos_compat.c)

`dos_detect`: `MZ` → MZ_EXE, `NE` → NE_WIN31, anything else → RAW_COM
(DOS's own rule).

`dos_load(a, img, len, out)`:
1. `.COM`: reject if `len > 64K - 0x100`; allocate `0x100 + len`
   8-bit cells byte-aligned; copy image to cell `0x100`; entry `0:0x100`.
2. MZ: `memcpy` header (avoids unaligned access); validate signature,
   `hdr_paras*16 <= len`, relocation table inside file, `blocks > 0`;
   `fsize = blocks*512 - (512 - last_bytes)` (with `last_bytes == 0`
   meaning a full 512); `data = fsize - hdr_size`; allocate `data`
   8-bit cells; copy; apply relocations
   (`word at hdr_size + seg*16 + off += load_para`, flat `load_para = 0`);
   entry `cs:ip` from header. Corrupt tables → `T_FALSE`; too small a
   pool → `T_UNKNOWN`.
3. NE → `T_FALSE` (Phase 2 stub).

`dos_int21h(s, ah, regs)` with `regs = [AX,BX,CX,DX,SI,DI,DS,ES]`:
- `09h`: `$`-terminated string at `DS:DX` (linear = `DS*16+DX`,
  20-bit wrapped, bounds-checked against region) → `putchar_fn`.
  Caps scan at 4096 chars; missing `$` → `T_FALSE`.
- `02h`: `DL` → `putchar_fn`.
- `3Dh/3Eh/3Fh/40h`: recognized, `T_UNKNOWN` (stubs).
- `4Ch`: sets `terminated`, `T_TRUE`.
- else: `T_FALSE`.

`putchar_fn` defaults to host `putchar()`; on 8086 it would be a BIOS
INT 10h teletype thunk. Preserved across `dos_load` if set beforehand.

## 5. Boot flow (boot.asm → stage2 → kmain)

1. BIOS loads 512-byte `boot.bin` at `0x7C00`, `DL` = boot drive.
2. Boot sets segments/stack, prints `G`, retries INT 13h AH=02h
   (3 tries) to load LBA-adjacent sector 2 (CHS 0/0/2) to `0x7E00`,
   prints `+`, far-jumps to `0x0000:0x7E00`. Failure prints `E`, halts.
3. `stage2.asm` (stub): prints `2`, halts. The real stage2 will call
   `kmain` (needs `ia16-elf-gcc`; host builds use `HOST_TEST`).
4. `kmain` (main.c): inits allocator on a 4K-word static pool, runs the
   cell/trit demo over serial (COM1 9600 8N1), halts.

Expected QEMU floppy output: `G+2`.
