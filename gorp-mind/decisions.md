# Decisions

- 2026-09-17: Pentium 4 / NetBurst is the oldest-supported CPU (supersedes 8086).
  32-bit protected mode + paging + SSE2 baseline. 8086 kept as historical appendix.
- 2026-09-17: Packed (not padded) bit-stream storage for 8/10/13-bit cells.
- 2026-09-17: Ternary logic = Kleene false/unknown/true; T_UNKNOWN never leaks
  past the kernel boundary into guest APIs.
- 2026-09-17: Full-scope docs use honest labels: v1-real / staged / epic / research.
- 2026-09-17: Compat strategy = frozen named program lists (Wine lesson), Linux
  shim targets static musl binaries first (WSL1 lesson).
- 2026-10-11: Project license = **Apache-2.0**. LICENSE file added at repo
  root; `SPDX-License-Identifier: Apache-2.0` headers on all 18 source files
  (.c/.h/.asm/.rs/.sh/.ld). Docs covered by root license. Rationale: permissive,
  explicit patent grant suits hardware-adjacent work, compatible with the
  third_party adoption rule (MIT/BSD/Apache-2.0 vendoring OK; GPL excluded from
  kernel tree unless project ever goes GPL). Copyright line: Cryonic7.
