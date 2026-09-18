# GorpOS — Compiler License Verification Sweep (Avenue 11)

**Status:** research deliverable. No implementation.
**Purpose:** gates the adoption decision in docs/COMPILERS.md, where
every candidate's license was marked `[unverified]`. Each license
below was checked against its canonical source (LICENSE/COPYING
file, README, or project homepage). Anything not confirmed from a
canonical or canonical-adjacent source is explicitly marked
`[unverified]`.

---

## 1. Per-project license verification

| Project | Canonical source | Confirmed license (SPDX) | Scope (what's covered) | Verification status |
|---|---|---|---|---|
| TinyCC (tcc) | [https://github.com/TinyCC/tinycc](https://github.com/TinyCC/tinycc) (unofficial mirror of mob dev branch; canonical repo is `http://repo.or.cz/tinycc.git`) | **LGPL-2.1** (whole distribution) | Compiler binaries (`tcc`), `libtcc`, and the runtime `libtcc1` — all under the same LGPL grant. Per a downstream per-file audit of the EXOS build, two peripheral files carry different terms (`il-opcodes.h` — GPL, only used by `il-gen.c`; the `tiny_impdef` section of `tcctools.c` — GPL, only under `TCC_TARGET_PE`); neither is in the x86-64/i386 ELF build path. | Confirmed (README states "TCC is distributed under the GNU Lesser General Public License (see COPYING file)"; GitHub SPDX detection = LGPL-2.1). GPL-carveouts per secondary audit only — re-verify before adopting. |
| z101/tcc fork | [https://github.com/z101/tcc](https://github.com/z101/tcc) | **LGPL-2.1** (inherited) | Same as upstream. | Confirmed (fork README repeats the LGPL notice verbatim). |
| mingodad/tinycc fork | [https://GitHub.com/mingodad/tinycc](https://GitHub.com/mingodad/tinycc) | LGPL-2.1 (inherited) | Same as upstream. | [unverified] — inherited from upstream; fork page not individually checked. |
| cproc | Canonical: `https://git.sr.ht/~mcf/cproc`; GitHub mirror: [https://github.com/michaelforney/cproc](https://github.com/michaelforney/cproc) | **ISC** | Main compiler under ISC. Carve-outs in the LICENSE file itself: `tree.c` (musl-derived) is **MIT**; `test/*` is **Unlicense** (public domain). QBE backend is a separate project (see below). | Confirmed (canonical LICENSE file read in full; README: "released under the ISC license"). |
| QBE | Canonical: `https://c9x.me/compile/` | **MIT** | The whole backend compiler. | Confirmed via authoritative distribution channels (Alpine Linux package metadata lists `License: MIT` with project URL `https://c9x.me/compile/`; the qbepy PyPI page states "QBE is developed by Quentin Carbonneaux and is also MIT licensed. See vendor/qbe/LICENSE"). c9x.me could not be fetched directly in this pass — direct tariff of the canonical tarball still worth one look at adoption time. |
| lcc (Fraser & Hanson) | [https://github.com/drh/lcc](https://github.com/drh/lcc) | **Custom noncommercial license** (SPDX: none — `LicenseRef-LCC-Noncommercial`) | The entire compiler distribution: "You may not sell lcc or any product derived from it in which it is a significant part of the value of the product." Free for "personal research and instructional use"; redistribution only with attribution. | Confirmed (canonical CPYRIGHT file read in full). **Verdict: DISQUALIFIED** — see §3. |
| erysdren/lcc fork | [https://github.com/erysdren/lcc/blob/HEAD/README.md](https://github.com/erysdren/lcc/blob/HEAD/README.md) | Custom noncommercial (inherited) | Fork explicitly retains the original CPYRIGHT; its own README documents the non-commercial terms and disclaims legal interpretation. | Confirmed — no relicensing occurred. |
| besm6/lcc fork | [https://github.com/besm6/lcc](https://github.com/besm6/lcc) | Custom noncommercial (inherited) | Build/maintenance fork; no license change found. | Confirmed-unchanged (no relicense claim found in returned sources). |
| chibicc | [https://github.com/rui314/chibicc](https://github.com/rui314/chibicc) | **MIT** | Whole compiler. | Confirmed (canonical LICENSE file read in full: "MIT License", Copyright (c) 2019 Rui Ueyama). |
| 8cc | Upstream: `https://github.com/rui314/8cc` (archived); task-named fork: [https://github.com/shinh/8cc](https://github.com/shinh/8cc) | MIT (reported) | Whole compiler. | [unverified] — no LICENSE file found in the canonical repo or its README; MIT is attested by the Arch Linux AUR `8cc-git` package metadata (upstream URL = rui314/8cc, `Licenses: MIT`) and by the offpepe/lambda-8cc vendored-copy notice ("All of the following projects are released under the MIT license … 8cc: By Rui Ueyama"). Two independent attestations, but no canonical grant text located. |
| cc65 | [https://github.com/cc65/cc65](https://github.com/cc65/cc65) | **zlib** | Whole suite: compiler, assembler, linker, archiver, simulator, and runtime libraries. | Confirmed via distribution metadata (GNU Guix `cc65` package: "Licenses: Zlib"; OpenHub: "zlib License (aka zlib/libpng)"; the 42bastian lynxcc fork documents the inherited root `LICENSE` as "cc65 zlib-style baseline"). The raw COPYING could not be fetched this pass — re-verify the file at adoption time. Historical note: the *pre-2.13-era* compiler carried a GPLv1-variant "freeware" license (EDM2 wiki); that is stale for the current tree but explains occasional confusion. |
| pcc (PortableCC) | [https://github.com/PortableCC/pcc](https://github.com/PortableCC/pcc) (project: `http://pcc.ludd.ltu.se/`) | **BSD + ISC mix** | Whole compiler tree; "The sources are available under a mix of permissive BSD and ISC style licenses. See `LICENSE.md` for an aggregate audit … Individual file headers remain authoritative." | Confirmed (canonical README "Licensing" section; corroborated by Wikipedia "BSD License" and T2 package metadata "License: BSD"). SPDX nuance: mixed BSD-2-Clause/BSD-3-Clause/ISC per file — an audit of `LICENSE.md` + file headers is advisable before embedding, but all terms are permissive. |
| SmallerC | [https://github.com/alexfru/SmallerC](https://github.com/alexfru/SmallerC) | **unknown** | — | [unverified] — GitHub classifies the repo as "Other License"; the 66-line `readme.txt` contains **no** license terms; the docs (`v0100/doc/smlrc.md`) contain no license section in returned sources. Two search batches yielded nothing new. **Treat as all-rights-reserved until the author clarifies.** This is the one open gate in the survey. |

---

## 2. GPL-vs-permissive analysis for an original OS project

What LGPL (tcc) vs. permissive (ISC/MIT/BSD/zlib) actually means
for the three GorpOS use cases.

### 2a. Building the GorpOS kernel with the compiler

**No license obligation either way.** The output of a compiler is not
a derivative work of the compiler — this is the same reason GCC can
compile proprietary kernels. LGPL-2.1's copyleft governs the
*library/compiler code*, not what it emits. So:

- Building the kernel with tcc (LGPL): zero license impact on
  GorpOS's own code. The kernel keeps whatever license the project
  chooses.
- Building with cproc/QBE, pcc, chibicc: also zero impact, trivially.

The only scenario that changes this is a compiler that *injects its
own code into the output* — which is exactly the libtcc1 runtime
question in §2b.

### 2b. Shipping the compiler's runtime library with the OS

This is where tcc differs from the permissive options. When the
kernel links `libtcc1` (tcc's runtime: `lib/libtcc1.c`, providing
helpers the generated code calls), the result is a work "containing
portions of the Library" under LGPL-2.1 §6. The obligations are:

1. Give prominent notice that libtcc1 is used and that it is LGPL.
2. Ship a copy of the LGPL and the libtcc1 source (or a written
   offer).
3. Give users a way to modify the library and relink — LGPL-2.1 §6
   offers three routes: (a) ship the OS's object files plus library
   sources so a user can relink, (b) use a shared-library mechanism,
   or (c) a written offer. For a statically-linked i386 kernel,
   route (a) means **publishing the kernel's .o files for relinking**
   — a real but entirely solvable compliance chore. It does **not**
   force the kernel to become LGPL/GPL.

For the permissive runtimes there is no such obligation: cproc
ships no runtime library at all; pcc's BSD/ISC files only require
keeping the copyright notices; chibicc has no runtime; cc65's zlib
runtime requires only notice preservation. So the permissive
candidates are *zero-friction* here, while tcc imposes a modest,
one-time compliance process (publish tcc sources + kernel objects +
written offer).

Practical consequence: tcc's libtcc1 is small and could be
**replaced by a from-scratch GorpOS runtime** (libgcc-style helper
functions are trivial and un-copyrightable in their function), which
would remove the LGPL surface entirely from the kernel while keeping
tcc as the code generator.

### 2c. Embedding the compiler in the OS later (the libtcc case)

COMPILERS.md flags `libtcc` as the future prize: an in-OS JIT /
self-hosting compiler. LGPL analysis for statically embedding
libtcc into a GorpOS component:

- Permitted: LGPL expressly allows linking the library into
  otherwise-proprietary (or differently-licensed) programs.
- Required: the LGPL component must remain replaceable/modifiable by
  the user — i.e., ship libtcc's source, the notice, and either the
  OS component's object files (for relinking) or a mechanism to swap
  the library. Again: it does **not** re-license the rest of the OS.
- Caveat: dynamic linking (the easy route under LGPL §6(b)) doesn't
  exist in early GorpOS, so plan on the object-file route (a).

The permissive alternatives have no equivalent: a permissively
licensed embedded compiler (e.g., a future in-OS cproc port, ISC)
could be vendored wholesale with no obligations beyond a copyright
notice. So the long-term self-hosting vehicle benefits more from a
permissive license than the near-term cross-compiler does — this is
one more argument for cproc+QBE (ISC+MIT) in the cell-native slot.

### The tcc MIT-relicensing watch

tcc's maintainers are actively relicensing the project under MIT
(the mob tree carries a RELICENSING file tracking per-file status;
several mirrors advertise the effort). If it completes, the entire
LGPL discussion above evaporates and tcc becomes as clean as pcc.
**Watch this at each toolchain review point; do not count on it.**
Until then, the compliance posture above (§2b/§2c) is the operative
one.

---

## 3. The lcc verdict: **confirmed disqualified**

The CPYRIGHT in [drh/lcc](https://github.com/drh/lcc) (read in
full) states, among other things:

> *"You may not sell lcc or any product derived from it in which it
> is a significant part of the value of the product."*

and frames the whole grant as "free for your personal research and
instructional use". Key points:

- This is **not** an OSI-approved or FSF-free license. It is a
  custom noncommercial-ish grant with a deliberately fuzzy "significant
  part of the value" test and a commercial-license path routed
  through the book publisher (Addison-Wesley).
- No fork has relicensed: erysdren/lcc (the most active modern fork)
  explicitly retains the original CPYRIGHT and documents the
  restriction itself; besm6/lcc is a build-system fork with no
  license change. The original authors have not relicensed lcc 4.2.
- Even if one argued the *output binaries* are fine (the compiler's
  output, not "lcc"), the disqualifier is structural: GorpOS's
  self-hosting milestone means the OS distribution would eventually
  **embed or ship the compiler itself**, and the license's "may not
  sell … any product derived from it" clause is a landmine for any
  OS that might ever be sold, bundled commercially, or built into
  commercial products. (Historical precedent: id Software needed a
  separate commercial arrangement for Quake 3's lcc-derived QVM
  compiler.)

**lcc is disqualified as the foundation.** It stays in the survey
only as a *teaching reference* (the book's retargeting interface
design). This clears the gate COMPILERS.md §5b left open — and the
answer is "no".

---

## 4. Updated adoption recommendations

Revisions to docs/COMPILERS.md's rankings in light of the license
findings. License was one of five ranking criteria (COMPILERS.md
§1.4); the technical analysis is unchanged.

### 4a. Near-term: cross-compiler emitting i386 against the GorpOS kernel ABI

COMPILERS.md had: 1) TinyCC, 2) SmallerC (co-first, license
pending), 3) pcc (fallback), 4) cproc/QBE (blocked on i386 backend).

| Rank | Pick | License finding | Why this rank |
|---|---|---|---|
| **1** | **TinyCC** (adopted fork: z101/tcc or TinyCC/tinycc mob) | LGPL-2.1 **confirmed** — workable (§2). MIT relicensing in progress; watch. | Technically strongest (mature i386 backend, self-hosting, fast edit→compile→boot loop, `libtcc` future). The license gate is now *clearable*: kernel builds unaffected; libtcc1 shipping needs only the §6 process (§2b), optionally eliminated by writing a clean-room runtime. |
| **2** | **pcc** (PortableCC) | **BSD+ISC confirmed** — zero friction. | Promoted from fallback to the *permissive alternate*. Genuine i386 target, deepest multi-target history, packaged by distros. Heavier/slower-moving and needs bison+flex to bootstrap, but if tcc's LGPL coupling is judged unacceptable at any review point, pcc is the drop-in answer. |
| — | **SmallerC** | **[unverified] — blocked.** | Demoted to *hold pending author clarification*. The single-pass i386 toolchain with its own preprocessor/linker/driver is still the best *technical* near-term fit (proven building FYS OS), but it cannot be adopted while its license is unknown; treat as all-rights-reserved until Alexey Frunze states terms. If he confirms a permissive grant, it re-enters as co-first with tcc on the merits. |
| 3 | **cproc + QBE** | ISC + MIT **confirmed** — license risk retired. | Still technically blocked: no i386 backend exists upstream (QBE: amd64/aarch64/riscv64 only), so the near-term job would require writing a QBE i386 backend first. |

Net change: **tcc becomes the sole first pick by default** — not
because it beat SmallerC on merit, but because SmallerC failed the
license gate and tcc cleared it. pcc is the no-copyleft safety net.
GCC remains the trusted bootstrap in all cases.

### 4b. Long-term: cell-native codegen

COMPILERS.md had: 1) cproc+QBE, 2) lcc (license pending), 3) fork
SmallerC/tcc one-pass codegen.

| Rank | Pick | License finding | Why this rank |
|---|---|---|---|
| **1** | **cproc + QBE** | ISC + MIT **confirmed** — license risk fully retired. | Unchanged on technical grounds and *strengthened* on licensing: the only candidate whose entire stack (frontend + backend + IL) is now verified permissive. The FPGC B32P3 retarget remains the template for the "gorp-cell" QBE backend + frontend data-model fork (§4 of COMPILERS.md). |
| 2 | **Fork of SmallerC or tcc one-pass codegen** | SmallerC: [unverified]; tcc: LGPL-2.1. | Promoted one slot by lcc's disqualification. Fastest hack to cell-aware codegen, worst maintainability — still the "only if QBE proves unsuitable" option. |
| — | **lcc** | Confirmed disqualified (§3). | Removed. Its book stays as the retargeting-design reference. |

### 4c. Teaching references (unchanged)

- **chibicc** — MIT confirmed; read for feature→codegen mapping.
- **lcc book** — retargeting theory; do not build on the code.
- **cc65** — zlib confirmed; the integrated-suite template
  (compiler + assembler + linker co-designed).
- **8cc ELVM branch** — word-machine precedent; license
  [unverified].

---

## 5. Open items / follow-ups

1. **[unverified] SmallerC license.** The critical open gate. The
   author (Alexey Frunze) is reachable via the repo and the
   [hackaday.io project page](https://hackaday.io/project/5569-smaller-c);
   a one-line statement of terms (or a LICENSE file) would settle
   it. Until then: no adoption.
2. **[unverified] 8cc license.** Two independent MIT attestations,
   no canonical grant text. Matters only for the ELVM-branch
   reference reading; not on the adoption path.
3. **cc65 COPYING.** License (zlib) confirmed via distro/fork
   metadata, but the raw COPYING could not be fetched this pass;
   one direct read at adoption time (cc65 is a template, not a
   candidate, so this is low priority).
4. **tcc per-file GPL carveouts** (`il-opcodes.h`,
   `tcctools.c#tiny_impdef`) — confirmed only via a secondary
   downstream audit; re-verify against the adopted fork's tree, and
   exclude those files from any GorpOS build of tcc.
5. **Watch: tcc MIT relicensing.** Check the RELICENSING file in
   the adopted fork at each toolchain review; completion would make
   tcc as clean as pcc.

---

## Sources

All URLs copied verbatim from tool results; only sources actually
opened or returned are cited. canonical-source fetches that failed
are noted inline.

- TinyCC — upstream mirror README (LGPL notice):
  [https://github.com/TinyCC/tinycc](https://github.com/TinyCC/tinycc)
- z101/tcc — README (LGPL notice):
  [https://github.com/z101/tcc](https://github.com/z101/tcc)
- mingodad/tinycc (fork page, from COMPILERS.md sources):
  [https://GitHub.com/mingodad/tinycc](https://GitHub.com/mingodad/tinycc)
- TinyCC mirrors documenting the MIT-relicensing effort and LGPL
  status:
  [https://github.com/wxfcc/tinycc](https://github.com/wxfcc/tinycc),
  [https://github.com/kodeumeister/tinycc](https://github.com/kodeumeister/tinycc),
  [https://github.com/bowlofstew/tinycc](https://github.com/bowlofstew/tinycc),
  [https://github.com/lunsir/tinycc](https://github.com/lunsir/tinycc)
- EXOS per-file license audit of a tcc build (secondary; GPL
  carveouts noted in §1):
  [https://github.com/jango73/exos/blob/HEAD/third/tinycc/README-EXOS.md](https://github.com/jango73/exos/blob/HEAD/third/tinycc/README-EXOS.md)
- cproc — canonical LICENSE file (ISC + MIT `tree.c` + Unlicense
  `test/*`), fetched via raw.githubusercontent:
  [https://raw.githubusercontent.com/michaelforney/cproc/master/LICENSE](https://raw.githubusercontent.com/michaelforney/cproc/master/LICENSE);
  README (ISC statement):
  [https://github.com/michaelforney/cproc/blob/HEAD/README.md](https://github.com/michaelforney/cproc/blob/HEAD/README.md)
- QBE — Alpine Linux package metadata (License: MIT):
  [https://pkgs.alpinelinux.org/package/v3.18/community/x86_64/qbe](https://pkgs.alpinelinux.org/package/v3.18/community/x86_64/qbe);
  qbepy PyPI page ("QBE … is also MIT licensed"):
  [https://pypi.org/project/qbepy/2026.2.1/](https://pypi.org/project/qbepy/2026.2.1/)
- lcc — canonical CPYRIGHT (noncommercial terms), fetched via
  raw.githubusercontent:
  [https://raw.githubusercontent.com/drh/lcc/master/CPYRIGHT](https://raw.githubusercontent.com/drh/lcc/master/CPYRIGHT);
  upstream repo [https://github.com/drh/lcc](https://github.com/drh/lcc);
  erysdren/lcc README (retains CPYRIGHT):
  [https://github.com/erysdren/lcc/blob/HEAD/README.md](https://github.com/erysdren/lcc/blob/HEAD/README.md);
  Wikipedia summary:
  [https://en.wikipedia.org/wiki/LCC_(compiler)](https://en.wikipedia.org/wiki/LCC_(compiler))
- chibicc — canonical LICENSE (MIT), fetched via
  raw.githubusercontent:
  [https://raw.githubusercontent.com/rui314/chibicc/master/LICENSE](https://raw.githubusercontent.com/rui314/chibicc/master/LICENSE)
- 8cc — AUR `8cc-git` metadata (Licenses: MIT, upstream
  `https://github.com/rui314/8cc`):
  [https://aur.archlinux.org/packages/8cc-git](https://aur.archlinux.org/packages/8cc-git);
  lambda-8cc vendored-copy notice ("8cc … released under the MIT
  license"):
  [https://github.com/offpepe/lambda-8cc](https://github.com/offpepe/lambda-8cc);
  fork page [https://github.com/shinh/8cc](https://github.com/shinh/8cc)
- cc65 — GNU Guix package (Licenses: Zlib):
  [https://packages.guix.gnu.org/packages/cc65/](https://packages.guix.gnu.org/packages/cc65/);
  OpenHub (zlib):
  [https://openhub.net/p/cc65](https://openhub.net/p/cc65);
  lynxcc fork license policy (inherited zlib baseline):
  [https://github.com/42bastian/lynxcc-fork](https://github.com/42bastian/lynxcc-fork);
  breadcraft bundling note (zlib):
  [https://github.com/farbfinsternis/breadcraft/blob/HEAD/resources/cc65/README.md](https://github.com/farbfinsternis/breadcraft/blob/HEAD/resources/cc65/README.md);
  repo [https://github.com/cc65/cc65](https://github.com/cc65/cc65)
- pcc — PortableCC README ("mix of permissive BSD and ISC style
  licenses"):
  [https://github.com/PortableCC/pcc](https://github.com/PortableCC/pcc);
  ronnya/pcc (BSD-style statement):
  [https://github.com/ronnya/pcc](https://github.com/ronnya/pcc);
  Wikipedia: [https://en.wikipedia.org/wiki/Portable_C_Compiler](https://en.wikipedia.org/wiki/Portable_C_Compiler);
  T2 package metadata (License: BSD):
  [http://t2linux.com/packages/pcc](http://t2linux.com/packages/pcc)
- SmallerC — repo page (GitHub classifies as "Other License"):
  [https://github.com/alexfru/SmallerC](https://github.com/alexfru/SmallerC);
  readme.txt (no license terms found), fetched via
  raw.githubusercontent:
  [https://raw.githubusercontent.com/alexfru/SmallerC/master/readme.txt](https://raw.githubusercontent.com/alexfru/SmallerC/master/readme.txt);
  docs [https://github.com/alexfru/SmallerC/blob/master/v0100/doc/smlrc.md](https://github.com/alexfru/SmallerC/blob/master/v0100/doc/smlrc.md)
  (no license section in returned sources)
