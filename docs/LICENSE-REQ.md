# LICENSE-REQ — Licensing requirements & inventory

Purpose: know exactly what is encumbered, what is clean, and what would need
clean-room reimplementation if we ever adopted it. User's lean is clean
ground-up reimplementation throughout; this document records what that
commitment costs us item by item.

**Headline: the repo currently has NO license.** Everything in it is original
work (written in-session, no vendored code), but with no LICENSE file it
defaults to all-rights-reserved — which technically means nobody, not even a
collaborator, may legally use, fork, or redistribute it. Fixing this is
action item #1 below.

## 1. Our own code — UNLICENSED (action required)

All of the following is original work authored for this project. No third-party
code is vendored anywhere in the tree (verified: `rust/Cargo.lock` contains
only the `gorp_os` crate itself; C sources are freestanding).

| Path | Content | Status |
|---|---|---|
| `src/boot/boot.asm`, `src/boot/stage2.asm` | Boot chain | original, unlicensed |
| `src/kernel/kernel.c`, `fb.c`, `alloc.c`, `dos.c`, `kmain.c`(entry) | Kernel C | original, unlicensed |
| `src/kernel/*.h` | Cell/allocator/trit headers | original, unlicensed |
| `src/boot/build.sh`, `linker.ld` | Build scripts | original, unlicensed |
| `rust/src/*.rs` | Rust mirror | original, unlicensed |
| `docs/*.md`, `USER_MANUAL.md`, `README.md` | All documentation | original, unlicensed |
| `gorp-mind/*` | Memory/scratchpads | original, unlicensed |

**Recommendation:** license the whole tree under **Apache-2.0** (or MIT — the
practical difference here is negligible; Apache-2.0 adds an explicit patent
grant, which suits a hardware-adjacent project). Rationale:

- Permissive licensing matches the clean-room ethos: anyone can reimplement
  from our docs without license friction, and we can accept contributions
  without contributor-license overhead.
- Copyleft (GPL) would be a defensible choice for an OS, but it would
  *require* clean-room discipline to flow the other way too — any GPL'd
  component we touch (SeaBIOS, coreboot, GPL'd drivers) would then be
  compatible, but every downstream user inherits the obligation. Given the
  stated lean is clean reimplementation rather than copyleft enforcement,
  permissive is the coherent pick.
- Dual-licensing (e.g. MIT for code, CC-BY-SA for docs) is an option if we
  want docs to stay share-alike like the OSDev wiki; but a single license
  keeps compliance trivial. Default to one license for everything unless
  there's a reason to split.

**Do not commit a LICENSE file until the project owner picks one.** Suggested
shortlist: Apache-2.0 (recommended), MIT, BSD-2-Clause, GPL-3.0-only (if
copyleft is ever wanted).

## 2. Build toolchain — no contamination

Tools execute; they don't ship. None of these licenses attach to our output.

| Tool | License | Notes |
|---|---|---|
| NASM | BSD-2-Clause | clean |
| GCC (cc1/as/ld) | GPL-3.0 | Compiler *output* is not GPL'd. Freestanding build avoids libgcc, but note: i386 has no hardware 64-bit divide — any `uint64_t` division in kernel C silently pulls in `__udivdi3`/`__umoddi3` from libgcc (GPL-3.0 **with** the GCC runtime exception, so still fine, but be aware). Prefer avoiding 64-bit division or supply our own helpers. |
| QEMU | GPL-2.0 | Tool use only; we don't link or ship it. |
| Rust toolchain (rustc/cargo) | MIT/Apache-2.0 | Host-side tests only. |
| `ld` linker script | n/a (our file) | — |

## 3. Knowledge sources — behavior is free, text is not

Clean-room reimplementation means: learn the *behavior* from the source, write
our own code and prose, never copy text, tables, or code samples verbatim.
Item-by-item:

| Source | Encrumbrance | What clean-room costs us |
|---|---|---|
| Intel SDM (P4/NetBurst) | Copyrighted text; CPU *behavior* not copyrightable | Nothing — writing our own GDT/IDT/paging code from the spec is industry-standard practice. Don't paste spec tables into docs verbatim; paraphrase. |
| OSDev Wiki | **CC-BY-SA 4.0** | **Real risk.** Do not copy wiki code samples into the repo — ShareAlike would attach to the surrounding work. Reimplement from understanding; link, don't paste. |
| VESA VBE 3.0 spec | VESA copyright; controlled distribution | Reimplement behavior (mode lists, function numbers are facts). Don't reproduce spec prose. |
| bitsavers (Voodoo3 docs) | Scanned historical docs; 3dfx defunct | Register facts are fine to use. |
| Ralf Brown's Interrupt List | Historical community doc | INT 21h/INT 10h behavior facts are fine. |
| FAT12/16/32 | Microsoft patents **expired 2019** | Clean. (exFAT patents are a separate matter; we're not implementing exFAT.) |
| POSIX/SUS (IEEE 1003.1) | Spec text copyrighted (IEEE/The Open Group) | Implementing syscalls/shell behavior from the spec is fine; don't quote large spec passages. |
| Multiboot spec (FSF) | Permissive | Clean. |
| NE/PE format knowledge | Facts about a 30-year-old format | Clean. |
| SeaBIOS | GPL-3.0 | We don't ship it — QEMU bundles it for our *testing*. If we ever ship a firmware image containing SeaBIOS, the GPL applies to that image. Our firmware plan (FULL_SCOPE) is our own code, so this stays a non-issue unless we change plans. |
| coreboot | GPL-2.0 | Same as SeaBIOS: payload *spec* knowledge is fine; forking coreboot code would GPL the fork. Current plan doesn't fork it. |

## 4. Pending: third-party code we might adopt

These are **not** in the repo. Each needs a license check *before* merge — that
check is avenue 11 (in progress), which feeds this section.

| Candidate | Use | License status |
|---|---|---|
| SmallerC / TinyCC / cproc+QBE / lcc / chibicc / 8cc / cc65 / pcc | Self-hosting compiler path | **pending** → `docs/COMPILERS_LICENSES.md` |
| musl (static binaries as compat corpus) | We run musl *binaries*; we don't link musl | Binaries' licenses are their authors'; running them is fine. If we ever port musl *libc source* as our libc: musl is MIT — clean. |
| Any future driver code (Voodoo3, etc.) | Reference only | Plan is clean-room from register docs; no adoption planned. |

**Adoption rule going forward:** no GPL-licensed code enters the kernel tree
unless the project license becomes GPL. Permissive (MIT/BSD/Apache-2.0/Zlib)
code may be vendored with its license text preserved in `third_party/`.
Public-domain/CC0 likewise. Anything else needs an explicit decision logged
in `gorp-mind/decisions.md`.

## 5. Action items

- [ ] **Pick a license for our code** (recommended: Apache-2.0) and add the
      `LICENSE` file. Until then the repo is all-rights-reserved by default.
- [ ] Fill in §4 from avenue 11 (`docs/COMPILERS_LICENSES.md`) when it lands.
- [ ] Standing rule: never paste OSDev Wiki code/text verbatim (CC-BY-SA).
- [ ] If 64-bit division ever appears in kernel C, resolve the libgcc helper
      question (supply our own or accept the GCC runtime exception).
- [ ] Revisit if we ever ship firmware containing SeaBIOS/coreboot (GPL trigger).
