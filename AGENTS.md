# AGENTS.md — GorpOS

Learning-first OS ("10-bit cells on 8086", now running on an IA-32/Pentium 4
baseline). Not performance-oriented; the packing/ternary experiments are the
point. Don't "optimize" the emulation away.

## Layout

- `src/kernel/` — portable C kernel core: `cell.h` (packed 8/10/13-bit cells
  over 16-bit words), `alloc.h`/`alloc.c` (bit-space region allocator),
  `trit.h` (Kleene three-valued logic), `dos_compat.c`, `fb.c`, `main.c`.
- `rust/` — `gorp_os` crate, `#![no_std]`, **faithful mirror of the C core**
  (`cell.rs`, `alloc.rs`, `trit.rs`). Behavior of C and Rust must stay
  identical (R-14). Rust is the only committed test suite.
- `src/boot/` — `boot.asm` (512-byte, CHS load, prints G/+/E) then
  `stage2.asm` (A20/GDT/protected mode/paging, prints "P4" to VGA) → far-jump
  to `kmain` at 0x10000 (`kernel.ld` forces `.text.entry` first).
- `docs/` — specs. `gorp-mind/` — session-surviving working memory
  (`decisions.md`, `open-questions.md`, `research-log.md`, `avenues.md`).
  Check these before planning; update them when you make decisions.

## Commands

```sh
# Rust mirror tests (the only in-tree test suite; 10 tests)
cd rust && cargo test

# Host-build C without the OS bits (no emulator needed)
gcc -Wall -Wextra -DHOST_TEST -I src/kernel src/kernel/main.c src/kernel/alloc.c -o ktest && ./ktest
# fb.c host build needs -DFB_HOST; dos_compat tests also link alloc.c

# Bootable image (POSIX-only; nasm, gcc -m32, ld, objcopy; optional qemu+socat)
cd src/boot && ./build.sh
```

Native Windows toolchain (installed 2026-09-17, all on PATH):
`cargo`/`rustc`/`rustup` (default `stable-x86_64-pc-windows-gnu` — MSVC is
absent), MSYS2 UCRT64 `gcc` 16.1.0 (`C:\msys64\ucrt64\bin`), NASM 3.02,
QEMU 11.1.0. Host C build + `cargo test` (10/10) verified.
`build.sh` CANNOT run natively — it needs an ELF toolchain (`gcc -m32`,
`ld -m elf_i386`, `objcopy`) plus `socat`; run it in WSL (see below).

No Makefile, no CI, no in-tree C tests (R-7: `make test` is a pending fix).
Build artifacts (`*.o/*.bin/*.elf/*.img`, `rust/target/`) are gitignored.

## WSL (Linux) toolchain — the boot build

WSL2 + Ubuntu is the only place `src/boot/build.sh` works. One-time setup:
```sh
wsl --install -d Ubuntu          # first time: needs a reboot, then create user
sudo apt update && sudo apt install -y build-essential gcc-multilib nasm \
     qemu-system-x86 socat       # gcc-multilib provides -m32
```
Then from inside WSL: `cd /mnt/c/Users/<you>/code/gorp/src/boot && ./build.sh`.
The script's QEMU step uses a UNIX-socket monitor — only works under WSL, not
native Windows (WinSocat/TCP monitor are not a drop-in).

## Rules that are easy to break

- **Mirror edits:** change `alloc`/`cell`/`trit` logic in C → apply the same
  change in `rust/` and run `cargo test`. The C (`-1`, garbage-on-bug) and Rust
  (`Option`/`bool`) error semantics deliberately differ (R-3/R-14); keep happy
  paths identical.
- **Widths are exactly 8, 10, 13.** Adding one = new constant + `valid_width()`
  + tests. Cell accessors assume width ≤ 16 (a cell spans ≤ 2 words).
- `region_put`/packed puts silently mask to width (hardware-register behavior,
  mirrored in Rust). Don't change to error.
- `rewidth` = alloc new + min-width-masked copy + free; on failure the old
  region must stay intact.
- `T_UNKNOWN` (trit encoding: 0=false, 1=unknown, 2=true, 3 reserved) must
  never leak out of the kernel boundary into guest APIs (decisions). "Unknown"
  means retry/defer, not error.
- DOS loader: it allocates its own 8-bit region (byte-aligned); callers must
  NOT pre-allocate. On any bad-format return, free the region (R-2).
- `HOST_TEST` in `main.c` swaps hardware paths (VGA/port I/O) for stdio —
  `io.h` must never be called in host builds.

## Gotchas

- QEMU verification uses VGA text + monitor, **not** `isa-debug-exit` (port
  0xF4 hangs after BIOS video calls on QEMU 9.2.1; R-22). CPU model is
  `qemu-system-i386 -cpu qemu32,+sse2` (no P4 model in QEMU 9+).
- SeaBIOS uses 0x7000–0x8FFF as scratch — never place PD/PT/stack there
  (R-20; current map: PD 0x5000, PT 0x6000, stack 0x9000). INT 13h AH=42h
  (EDD) hangs on the SeaBIOS floppy path; boot uses CHS.
- Interrupts must stay OFF until IDT + PIC remap + PIT exist (R-18). Any
  `sti` before that triple-faults.
- Docs lag the code. Trust `src/`; for status use `docs/ROADMAP.md` +
  `docs/REVIEW.md`. Known stale: `docs/ARCHITECTURE.md` §1 (R-13, still says
  "padded"), `docs/INTERNALS.md` §5 (describes the old G+2 8086 boot flow).
  `INTERNALS.md` is the exact-algorithms doc: if code and doc disagree, code
  wins and the doc must be fixed.
- Honesty convention (REVIEW audit): never claim Windows 3.1/DOS/graphics/
  Unix-compat features beyond what the code actually does; stubs return
  `T_UNKNOWN`.

## Entrypoints

- Boot → `stage2.asm` → `kmain()` in `src/kernel/main.c:96` (target) /
  `src/kernel/main.c:85` (HOST_TEST). Allocator inits on a 4K-word static pool.
- Port of `dos_compat.c` to Rust is the natural next mirror (only the
  `putchar` hook is platform-specific) — see `rust/README.md`.