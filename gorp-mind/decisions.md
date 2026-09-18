# Decisions

- 2026-09-17: Pentium 4 / NetBurst is the oldest-supported CPU (supersedes 8086).
  32-bit protected mode + paging + SSE2 baseline. 8086 kept as historical appendix.
- 2026-09-17: Packed (not padded) bit-stream storage for 8/10/13-bit cells.
- 2026-09-17: Ternary logic = Kleene false/unknown/true; T_UNKNOWN never leaks
  past the kernel boundary into guest APIs.
- 2026-09-17: Full-scope docs use honest labels: v1-real / staged / epic / research.
- 2026-09-17: Compat strategy = frozen named program lists (Wine lesson), Linux
  shim targets static musl binaries first (WSL1 lesson).
