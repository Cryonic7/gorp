# GorpOS User Manual

*Version 0.1 — learning-first experimental OS. Nothing here is finished;
everything here is honest about what it is.*

---

## 1. What is GorpOS?

GorpOS is an experiment in asking "what if": what if memory were made of
**10-bit cells** instead of 8-bit bytes? What if the kernel thought in
**three-valued logic** (true / false / *unknown*) instead of boolean?
Can you still boot that on a real 1978 CPU, the Intel 8086, and can you
still run old DOS programs on top of it?

Short answers: the 10-bit cells are *emulated* (the 8086 can't do
otherwise), the ternary logic is a kernel convention, and DOS programs
run through a translation shim. This manual explains each piece at the
level where you can actually use and modify it.

### 1.1 The one big idea to hold in your head

Everything below the kernel's "cell layer" is ordinary 16-bit words
(the 8086's natural unit). Everything above it pretends memory is a
stream of cells that are 8, 10, or 13 bits wide, packed back-to-back
with no gaps. The allocator is the translator between those two views.

```
  your code sees:   | 10 bits | 10 bits | 10 bits | ...
  hardware stores:  |---- 16-bit word ----|---- 16-bit word ----| ...
```

You pay for this in speed (every access is shift-and-mask). That is
the accepted cost of the experiment.

---

## 2. Concepts

### 2.1 Cells and widths

A **cell** is the kernel's unit of memory. Its **width** is one of three
discrete modes:

| Width | Bits | Why it exists |
|-------|------|---------------|
| 8     | 8    | Compatibility: DOS programs think in bytes |
| 10    | 10   | The experiment: the "native" GorpOS cell |
| 13    | 13   | Stress test: a non-power-of-two width for the packing logic |

Widths are **per-region, immutable**. If you need a different width,
you `rewidth` — which allocates a new region, copies with conversion,
and frees the old one.

### 2.2 Regions

A **region** is an allocation: a run of `count` cells, all one width.
Regions are described by `{width, cells, bit_base, bit_len}` and live
in a flat bit-space managed by first-fit search.

Key rules:
- Regions never overlap.
- Freeing a region makes its bits reusable immediately (no separate
  coalescing step — the scanner just sees the gap).
- A region's `bit_base` is an arbitrary *bit* offset unless you asked
  for alignment (the DOS loader asks for 8-bit alignment).

### 2.3 Ternary logic (trits)

The kernel's boolean type has three values:

| Value | Meaning | Typical use |
|-------|---------|-------------|
| `T_TRUE` | yes | operation succeeded |
| `T_FALSE` | no | operation failed / unsupported |
| `T_UNKNOWN` | don't know | recognized but unimplemented; *retry or defer* |

Logic follows Kleene's three-valued tables:

```
NOT:  ¬T=True→False, False→True, Unknown→Unknown

AND:          T  U  F        OR:           T  U  F
     T |      T  U  F             T |      T  T  T
     U |      U  U  F             U |      T  U  U
     F |      F  F  F             F |      T  U  F
```

Converting to plain C `bool`: `true → T_TRUE`, `false → T_FALSE`, and
`T_UNKNOWN → false` (a stricter kernel could trap instead; ours doesn't
yet — see Limitations).

### 2.4 What "DOS compatible" means here

It does **not** mean GorpOS *is* DOS. It means GorpOS can **load** DOS
programs (`.COM` files and MZ `.EXE` files) and **translate** a small
set of DOS service calls (INT 21h) into its own kernel calls. The 8086
CPU executes the program's instructions natively (it's already an 8086);
only the *operating system services* are emulated.

Supported today: `AH=09h` (print `$`-terminated string), `AH=02h`
(print character), `AH=4Ch` (exit). File calls (`3Dh/3Eh/3Fh/40h`) are
recognized stubs that report "unknown" for now.

---

## 3. Building

### 3.1 What you need

| Tool | For | Install |
|------|-----|---------|
| `gcc` | host tests of C code | already on most Linux |
| `cargo` | Rust mirror tests | [rustup.rs](https://rustup.rs) |
| `nasm` | boot sector assembly | `apt install nasm` |
| `qemu-system-i386` | boot the image | `apt install qemu-system-x86 |

### 3.2 Host tests (no emulator needed)

```sh
cd ~/workspace/os-project

# allocator + cell packing + trit (C)
gcc -Wall -Wextra -I src/kernel -o /tmp/test_alloc /tmp/test_alloc.c src/kernel/alloc.c
# (test sources live in /tmp for this session; see §3.4)

# DOS loader + INT 21h shim (C)
gcc -Wall -Wextra -I src/kernel -o /tmp/test_dos /tmp/test_dos.c \
    src/kernel/alloc.c src/kernel/dos_compat.c

# kernel demo entry (C)
gcc -Wall -Wextra -DHOST_TEST -I src/kernel -o /tmp/ktest \
    src/kernel/main.c src/kernel/alloc.c && /tmp/ktest

# Rust mirror
cd rust && cargo test
```

Expected: all tests print `ok` / `PASS`; Rust reports `10 passed`.

### 3.3 Building the bootable image

```sh
cd src/boot
chmod +x build.sh
./build.sh
# assembles boot.bin (must be exactly 512 bytes, ends 55 AA),
# concatenates stage2.bin -> gorpos.img
qemu-system-i386 -fda gorpos.img -boot a
```

**What you should see** in the QEMU window, in order:

| Character | Meaning |
|-----------|---------|
| `G` | boot sector running |
| `+` | stage 2 loaded from disk |
| `2` | stage-2 stub running |

So `G+2` = everything worked. `GE` = disk read failed.

### 3.4 Test sources

The C test programs used in §3.2 were written during development and
kept in `/tmp` for this session (`test_alloc.c`, `test_dos.c`). They
are not yet committed to the repo — see REVIEW.md item R-7.

---

## 4. Using the allocator (C)

```c
#include "alloc.h"

static uint16_t pool[1024];      // 2KB physical backing
static allocator_t a;

alloc_init(&a, pool, 1024);

// 1. Allocate 100 ten-bit cells
int r = alloc_cells(&a, WIDTH_10, 100);
if (r < 0) { /* out of memory or bad args */ }

// 2. Read/write cells (values are masked to the width)
region_put(&a, r, 0, 0x3FF);                 // max 10-bit value
uint16_t v = region_get(&a, r, 0);           // v == 0x3FF
region_put(&a, r, 1, 0xFFFF);                // masked to 0x3FF on store

// 3. Change width: 10 -> 8 (keeps low 8 bits)
int r2 = alloc_rewidth(&a, r, WIDTH_8);      // old region freed
// v = region_get(&a, r2, 0);               // == 0xFF

// 4. Byte-aligned region (needed for DOS-style linear addressing)
int rb = alloc_cells_aligned(&a, WIDTH_8, 256, 8);

// 5. Free
alloc_free(&a, r2);
```

**Rules to remember:**
- `alloc_cells` returns `-1` on failure — always check.
- `region_get/put` with an out-of-range index is undefined in C
  (the Rust mirror returns `Option`/`bool` instead).
- `rewidth` failure (`-1`) leaves the original region intact.
- Widths are exactly `8`, `10`, `13` — anything else is rejected.

### Rust equivalent

```rust
use gorp_os::alloc::{Allocator, WIDTH_10};

let mut pool = [0u16; 1024];
let mut a = Allocator::new(&mut pool);
let r = a.alloc_cells(WIDTH_10, 100).expect("oom");
a.put(r, 0, 0x3FF);
assert_eq!(a.get(r, 0), Some(0x3FF));
let r2 = a.rewidth(r, 8).expect("rewidth failed");
a.free(r2);
```

---

## 5. Using ternary logic

```c
#include "trit.h"

trit_t t = trit_and(T_TRUE, T_UNKNOWN);  // == T_UNKNOWN
if (t == T_TRUE) { /* definitely yes */ }
else if (t == T_FALSE) { /* definitely no */ }
else { /* unknown: retry, defer, or ask */ }

int b = trit_to_bool(T_UNKNOWN);         // == 0 (false)
trit_t u = bool_to_trit(1);              // == T_TRUE
```

Kernel convention: functions that *can* be indeterminate return
`trit_t` instead of `int`. `T_UNKNOWN` is not an error — it means "I
recognize this, but I can't answer yet."

---

## 6. Using the DOS loader

```c
#include "dos_compat.h"

static uint16_t pool[4096];
static allocator_t a;
alloc_init(&a, pool, 4096);

// img = bytes of a .COM or MZ .EXE file, len = its length
dos_session_t s;
s.putchar_fn = my_putchar;      // optional; defaults to putchar()
trit_t rc = dos_load(&a, img, len, &s);
if (rc == T_TRUE) {
    // s.region_id holds the image, s.cs:s.ip is the entry point
    uint16_t regs[8] = {0};
    regs[6] = 0;                // DS
    regs[3] = 0x100;            // DX -> "$"-terminated string for AH=09h
    dos_int21h(&s, 0x09, regs); // prints it
    dos_int21h(&s, 0x4C, regs); // "exit": s.terminated == 1
}
dos_unload(&s);                 // frees the region
```

Return values: `T_TRUE` = loaded, `T_FALSE` = bad/unsupported format
(includes NE executables), `T_UNKNOWN` = valid but needs more memory.

**INT 21h register layout:** `regs[0]=AX, [1]=BX, [2]=CX, [3]=DX,
[4]=SI, [5]=DI, [6]=DS, [7]=ES`. `AH` is passed separately as the
`ah` argument.

---

## 7. Hardware quick reference (8086)

| Device | Ports | Notes |
|--------|-------|-------|
| PIC 8259A | 0x20/0x21, 0xA0/0xA1 | remap IRQs to 0x20–0x2F; EOI = 0x20 |
| PIT 8253 | 0x40–0x43 | 1193182 Hz base; ch0 = IRQ0 |
| Keyboard 8042 | 0x60/0x64 | IRQ1; Set-1 scancodes |
| Serial COM1 | 0x3F8–0x3FF | 8250, 9600 8N1; debug console |
| VGA text | 0xB8000 (mem) | 80×25, char+attribute |

Full init sequences: `docs/hardware/8086.md`. Porting to x86_64 /
AArch64 / RISC-V: `docs/hardware/porting.md`.

---

## 8. Limitations (read before believing anything)

1. **10-bit is emulated.** There is no 10-bit hardware here; the 8086
   sees 16-bit words. "Native 10-bit" would require custom silicon.
2. **Slow by design.** Every cell access is shift+mask; packed 13-bit
   is the slowest. This is the experiment, not a bug.
3. **DOS support is a shim.** `.COM`/MZ load and 4 INT 21h functions.
   No real filesystem, no PSP, no memory management for DOS programs,
   no protected mode, no Windows 3.1 (stubs only, Phase 2).
4. **No privilege separation.** Everything runs at the same level;
   a loaded DOS program can touch any memory — exactly like real DOS.
5. **No scheduler, no processes** yet — `kmain` runs a demo and halts.
6. **The boot sector is hand-verified, not yet QEMU-tested** in this
   environment (no emulator installed here); `build.sh` checks size
   and signature before concatenating.
7. **Unix V4 compat** is API-shim scope only (recompile V4-style
   programs against our libc), not binary compatibility with 1975
   PDP-11 binaries.

---

## 9. FAQ

**Q: Why 13-bit? That's weird.**
A: Deliberately. 8 and 10 are the "useful" widths; 13 exists to prove
the packing math handles non-power-of-two widths crossing word
boundaries. If 13 works, the scheme is generic.

**Q: Can I add a 12-bit or 24-bit mode?**
A: Yes — add the constant, extend `valid_width()`, and add tests.
Note `cell_get_packed` assumes width ≤ 16 (a cell spans at most 2
words); wider cells need the accessors reworked.

**Q: Why does `region_put` silently mask instead of erroring?**
A: Design choice: hardware registers behave this way (extra bits fall
off). The Rust mirror matches.

**Q: Where's the scheduler / filesystem / network?**
A: Not built yet. The roadmap (`docs/ROADMAP.md`) stages them after
the boot + allocator + DOS-shim foundations.

**Q: Is this secure?**
A: No — see §8 item 4 and `docs/REVIEW.md`. It's a learning kernel.

---

## 10. Glossary

- **Cell** — the kernel's logical memory unit (8/10/13 bits).
- **Region** — an allocation: N cells of one width.
- **Trit** — a ternary truth value (true/false/unknown).
- **Shim** — translation layer (INT 21h → kernel calls).
- **Flat model** — DOS segments treated as linear addresses from 0.
- **Stage 2** — second-stage bootloader loaded by the boot sector.
- **kmain** — kernel C entry point.
