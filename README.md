# GorpOS — Experimental 10-bit OS for 8086

Learning-first operating system project. Goal: explore what a 10-bit-based
architecture feels like, even on 8086 hardware that is natively 8-bit-byte,
16-bit-word.

This repo is a scaffold and design space, not a bootable OS yet.

## Layout

- `docs/ARCHITECTURE.md` — core design tensions and how we fake 10-bit on 8086
- `docs/ROADMAP.md` — staged build plan
- `src/boot/` — boot sector / loader notes (8086 real mode)
- `src/kernel/` — kernel sketch: memory, ternary logic, syscall shim
- `src/lib/` — helpers

## Big ideas being explored

1. **10-bit base on 8086:** 8086 has 8-bit bytes and 16-bit registers.
   10-bit cells must be emulated by packing into 16-bit words (waste 6 bits)
   or across word boundaries (complex, slower). We choose explicit tradeoffs.

2. **Dynamic bit-width memory assignment:** allocator hands out regions
   tagged with width (e.g. 8, 10, 16) and tracks packing.

3. **Binary-to-ternary boolean at kernel level:** kernel natively
   understands 3-valued logic (true/false/unknown) and can convert to/from
   binary for compat.

4. **Unix V4 (1975) compat layer:** V4 ran on PDP-11. We aim for a syscall-
   level shim, not binary compat — translate V4-ish syscalls to our kernel.

## Status

Stage 0: design and scaffolding. No bootable image yet.
