# Rust mirror — `gorp_os` crate

Faithful Rust port of the portable C kernel core:

| Rust module      | C origin                      |
|------------------|-------------------------------|
| `src/trit.rs`    | `src/kernel/trit.h`           |
| `src/cell.rs`    | `src/kernel/cell.h` (packed)  |
| `src/alloc.rs`   | `src/kernel/alloc.h`/`alloc.c`|

Deliberately kept behavior-identical to the C, including:
- u16 truncation in the low-word mask of packed puts,
- first-fit bit-space allocation with optional bit alignment,
- `rewidth` = alloc new + min-width-masked copy + free (old intact on failure).

The crate is `#![no_std]` (uses only `core`), so it can target a real
kernel build later. Tests run on the host harness:

```
cargo test
```

**Not mirrored (yet):** `dos_compat.c` (DOS loader / INT 21h shim),
`main.c`, `io.h`, and the boot sector remain C/asm-only. The DOS loader
is the natural next port — its logic is portable, only the `putchar`
hook is platform-specific.
