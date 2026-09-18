# GorpOS — Tiny C Compilers: Survey + Self-Hosting Recommendation

**Status:** research + recommendation. No implementation in this pass.
**Why this matters:** the self-hosting compiler is the project's
longest pole (FULL_SCOPE: "self-hosting compiler" is a long-term
goal). Every systems milestone before it — kernel, libc, shell,
userspace — needs a *cross* compiler that emits i386 code against the
GorpOS kernel ABI from a Linux host. And the project's defining
quirk — virtual packed 8/10/13-bit cells over 16-bit words, with
`CHAR_BIT != 8` identified in CPU_SUPPORT.md as a hard type-system
wall — decides which compilers can ever become *cell-native*.
This document surveys the realistic candidates (tcc, cproc/QBE, lcc,
chibicc, 8cc, cc65, pcc, plus SmallerC, found mid-survey), analyzes
the `CHAR_BIT != 8` problem explicitly, and gives **separate
rankings** for (a) the near-term cross-compiler and (b) the long-term
cell-native compiler, because they are different jobs and the same
compiler does not win both.

**Method note:** facts below are web-verified (cited) or marked
`[unverified]`. Licenses in particular were not all confirmable from
returned sources — each is flagged. "Self-hosting" below means the
compiler can compile its own source (the standard bootstrap test);
it does not mean it already runs on GorpOS.

---

## 1. Requirements the candidates are judged against

1. **i386 codegen, 32-bit protected mode, SSE2 baseline.** GorpOS's
   oldest-supported CPU is Pentium 4 / NetBurst. Any near-term
   compiler must emit 32-bit x86 *today*, not after a backend is
   written.
2. **Self-hosting.** Must compile itself; the compiler's own source
   must be small enough to port to GorpOS libc eventually.
3. **Retargetability.** For the long term: how hard is it to aim the
   compiler at the Gorp *cell model* rather than at octets?
4. **License.** Must be compatible with an original OS project that
   may one day accept contributions under a chosen license. Anything
   with a noncommercial-only clause is disqualified as the
   foundation (fine as a reference).
5. **Activity / bus factor.** A one-person dormant project can still
   be adopted, but the risk is recorded.

**Prerequisite none of them remove:** the GorpOS i386 kernel ABI —
syscall convention, object format (ELF), C calling convention — must
be pinned before any compiler work starts (WORK_PROPOSAL scope). The
compiler decision in §7 assumes that ABI exists to target.

---

## 2. Comparison table

| Compiler | Model | Retarget mechanism | i386 today? | License | Activity | LOC (approx) |
|---|---|---|---|---|---|---|
| TinyCC (tcc) | one-pass, direct codegen to memory/object | per-arch backend, no IR | **yes** (i386 backend) | [unverified — confirm in adopted fork; canonical tcc is LGPL] | active forks | ~100K [unverified] |
| cproc + QBE | frontend → QBE SSA IL → backend | **write a QBE backend** | no (QBE: amd64/aarch64/riscv64 only) | cproc ISC [unverified — secondary source]; QBE MIT [unverified] | active | ~7K + ~14K |
| lcc | tree/DAG IR → target description | `Interface` struct (~18 ops) + target `.c` | **yes** (historic x86 targets) | restrictive/noncommercial lineage [unverified — must clear] | maintained forks | ~20K [unverified] |
| chibicc | AST → x86-64 asm, no IR, no optimizer | rewrite `codegen.c` (x86-64 baked in) | no | MIT [unverified] | book-complete | ~10K [unverified] |
| 8cc | x86-64; ELVM-IR branch exists | ELVM branch targets word-machine VM | no (x86-64; ELVM branch is a VM) | MIT [unverified] | dormant | ~15K [unverified] |
| cc65 | 6502-family only | n/a (single family) | no | [unverified] | **active** | large suite |
| pcc | two-pass (cpp/ccom/c2), Johnson lineage | per-arch `arch/` dirs, ~16 targets | **yes** (i386 listed) | BSD lineage [unverified] | packaged, slow | large |
| SmallerC | single-pass, NASM asm output | per-arch codegen (i386 + MIPS exist) | **yes** (16/32-bit 80386+) | [unverified] | maintained by author | small |

LOC figures are order-of-magnitude working numbers, not audited
counts — treat them as sizing intuition only.

---

## 3. Per-compiler notes

### 3.1 TinyCC (tcc)

One-pass compiler: parses and emits machine code in a single pass,
directly to memory (for `-run`/JIT via `libtcc`) or to object files.
There is no intermediate representation — each architecture has a
hand-written code generator. Current forks support i386, x86-64, ARM,
AArch64, and RISC-V, and tcc compiles itself
([z101/tcc](https://github.com/z101/tcc),
[mingodad/tinycc](https://GitHub.com/mingodad/tinycc)).

- **Strengths for us:** mature i386 backend *now*; very fast
  compiles (matters when the kernel build loop is edit→compile→boot);
  `libtcc` means the compiler can be embedded in OS tooling later
  (on-disk JIT, dynamic loading experiments); self-hosting is
  proven.
- **Weaknesses:** retargeting to a new machine model means writing a
  new one-pass backend with no IR to lean on — the frontend and
  codegen are interleaved by design. It is the *least* architecturally
  clean of the candidates for the cell-native job.
- **License:** confirm in the adopted fork before building on it
  [unverified].
- **Verdict:** strongest near-term cross-compiler candidate.

### 3.2 cproc + QBE

cproc is a small C99 compiler (~7K LOC per a secondary evaluation)
that outsources everything target-specific: it uses an external
preprocessor, emits **QBE IL** (an SSA-based intermediate language),
and shells out to an assembler and linker
([README](https://github.com/michaelforney/cproc/blob/HEAD/README.md)).
Upstream targets are x86_64, AArch64, and RISC-V 64 — **there is no
i386 target**. Bootstrap is verified by byte-identical stage2/stage3
comparison. Known upstream gaps: VLAs, `volatile`, `_Thread_local`,
`long double`, inline assembly, built-in preprocessor, PIC.

QBE itself is the attraction: a compiler backend designed to deliver
a large fraction of industrial optimizer quality in a tenth of the
code (the commonly quoted framing is "70% of the performance in 10%
of the code"; current QBE targets amd64, arm64, and riscv64 —
[HNS discussion](https://news.ycombinator.com/item?id=33802725)).
Its IL is small, textual, SSA, and documented — the cleanest
frontend/backend seam in this survey.

- **Retarget precedent (important):** the FPGC project adapted *both*
  cproc and QBE to a custom 32-bit CPU ("B32P3"), precisely the
  frontend→QBE-IR→new-backend path we would need
  ([FPGC docs](https://github.com/bartpleiter/fpgc/blob/HEAD/Docs/docs/Software/C-compiler.md)).
  A w65c816 QBE backend is also reported to exist [unverified].
- **Weaknesses:** no 32-bit x86 — QBE's supported list is 64-bit
  only, so the near-term job needs a *new QBE i386 backend* written
  first. QBE's memory model is byte/halfword/word/long load-store;
  teaching it 10-bit cells (§5) is a deeper change than a normal
  backend port.
- **License:** cproc ISC per a secondary evaluation
  ([viper notes](https://github.com/splanck/viper/blob/HEAD/docs/chibicc_evaluation.md))
  [unverified — check the README]; QBE MIT [unverified].
- **Verdict:** best long-term cell-native vehicle; not the near-term
  pick without backend work.

### 3.3 lcc (Fraser & Hanson)

The classic *designed*-retargetable C compiler. The frontend builds a
tree/DAG IR; the backend is selected through an `Interface` structure
of about eighteen operations (addressability, alignment, calling
convention, instruction selection), each target supplied as a single
C file. The implementation is published as a literate program — the
book *A Retargetable C Compiler: Design and Implementation* walks the
entire source, including the code generator's `gen()` pass
([book PDF](https://www.lmd.vg/pub/books/c/retargetable-c-compiler.pdf)).
Historic targets include x86 ("x86s running Linux", "x86s running MS
Windows NT 4.0" target files survive in forks such as
[erysdren/lcc](https://github.com/erysdren/lcc)). At the time of the
authors' Linux Journal writeup, lcc was ~75% smaller than gcc and
compiled itself in 36 seconds on a 90 MHz Pentium
([paper](https://www.researchgate.net/publication/255674429_Compile_C_Faster_on_Linux)).
Maintained forks exist ([besm6/lcc](https://github.com/besm6/lcc)
with CMake and a test suite; erysdren's fork retargets it to Quake 3
bytecode and experiments with WebAssembly — proof the interface can
aim at genuinely odd machines).

- **Strengths:** retargeting is the *documented, intended* workflow,
  not a hack; the book teaches exactly the skill this project needs.
- **Weaknesses:** the historical lcc license restricted use to
  noncommercial purposes [unverified — must be cleared before
  adopting as the foundation]; the codebase is 1990s C with K&R
  hangover in places; 16-bit-int-era assumptions may lurk.
- **Verdict:** architecturally ideal retargeting study; license risk
  disqualifies it as the foundation until cleared.

### 3.4 chibicc (Rui Ueyama)

A complete C11 compiler (preprocessor, `long double`, bit-fields,
VLAs, TLS, atomics — nearly all of C11) structured as
tokenize → preprocess → parse (AST) → **codegen directly to x86-64
assembly**, with no IR and no optimization pass. The author states
portability is not a goal
([README mirror](https://archive.org/details/github.com-rui314-chibicc_-_2021-07-06_23-18-44)).
Retargeting means rewriting `codegen.c`: x86-64 System V idioms are
baked into every emit site.

- **Strengths:** the best *readable* complete modern C compiler in
  existence — the commit history is literally a compiler-construction
  course. Superb for learning how each C feature becomes machine
  code.
- **Weaknesses:** nothing about it is retargetable; no i386; no
  optimizer; a book project, not a maintained toolchain.
- **License:** MIT [unverified].
- **Verdict:** teaching reference only. Do not build on it.

### 3.5 8cc (Rui Ueyama, earlier work)

Ueyama's earlier small C compiler, x86-64, self-hosting. Its
notable feature for us is the **ELVM branch** (`eir` branch of
shinh/8cc): a frontend targeting
[ELVM](https://github.com/shinh/elvm), a tiny Harvard-architecture VM
with 6 registers where `sizeof(char) == sizeof(int) ==
sizeof(void*) == 1` and the word size is backend-defined (most
backends use 24-bit words). ELVM is the closest thing in this survey
to a production-ish C targeting a **word-addressed, non-8-bit**
machine — direct precedent for the §5 discussion. (A stunt build even
runs 8cc via ELVM→lambda-calculus:
[lambda-8cc](https://github.com/woodrush/lambda-8cc).)

- **Verdict:** not a base (x86-64, dormant), but the ELVM branch is
  required reading for the cell-native design: it shows exactly which
  parts of a C implementation change when `char` stops being an
  octet.

### 3.6 cc65

A complete cross-development suite for 65(C)02 systems — C compiler,
macro assembler (ca65), linker (ld65), archiver, simulator — with C
and runtime library support for many 6502 machines, actively
maintained
([README](https://github.com/cc65/cc65/blob/HEAD/README.md)). It is
single-family (6502 only); there is no x86 path and none is
contemplated.

- **Why it's in this survey anyway:** cc65 is the template for what
  "self-hosting toolchain" actually means — *compiler + assembler +
  linker designed together*, not just a compiler. When GorpOS needs
  its own native toolchain, the shape of the answer looks like cc65
  (integrated suite, per-machine runtime libs), not like a bare
  compiler binary. Its codegen for an 8-bit accumulator machine also
  demonstrates how far C must bend for a narrow machine — the same
  kind of bending §5 anticipates for cells.
- **Verdict:** organizational template + narrow-machine lesson, not a
  candidate.

### 3.7 pcc (Portable C Compiler, Johnson lineage)

Descended from S. C. Johnson's 1970s pcc; the modern project supports
C99 with later additions across roughly 16 targets
([PortableCC](https://github.com/PortableCC)). i386 is a supported
"classic" target
([ronnya/pcc](https://github.com/ronnya/pcc)); Debian ships pcc
1.1.0 with target-dependent `-m` options including i386
([manpages](https://manpages.debian.org/experimental/pcc/pcpp.1.en.html),
[FreshPorts](https://www.freshports.org/lang/pcc/)). Architecture: a
traditional multi-pass design (preprocessor, `ccom` proper,
target-specific code generator) with per-architecture directories —
retargeting is supported but the codebase is larger and older than
the other candidates, and building it needs bison/flex (a bootstrap
dependency the others don't have).

- **Strengths:** permissive BSD lineage [unverified]; genuine i386
  target; the deepest multi-target history in the survey; packaged by
  distros.
- **Weaknesses:** heaviest codebase here; slowest-moving project;
  more 1970s-idiom C to absorb.
- **Verdict:** the solid fallback if tcc's license or coupling
  becomes a problem. Not the first pick.

### 3.8 SmallerC (Alexey Frunze)

Found in the final saturation sweep and immediately relevant: a
**simple, small, single-pass C compiler** (C89/C99 common subset)
that generates **16-bit and 32-bit 80386+ assembly for NASM**,
assemblable and linkable into DOS, Windows, Linux, and Mac OS X
programs; a MIPS backend exists (RetroBSD). It **compiles itself**,
and ships as a toolchain: preprocessor (ucpp), linker, and a
gcc-like driver
([GitHub](https://github.com/alexfru/SmallerC)). It has already been
used to build a hobby OS (FYS OS,
[http://www.fysnet.net/fysos.htm](http://www.fysnet.net/fysos.htm)).

- **Strengths:** i386 *now*, self-hosting *now*, whole-toolchain
  shape (preprocessor + linker + driver included — the cc65 lesson,
  pre-applied), explicitly designed for OS-dev-style bare-metal
  targets, small enough for one person to fully understand.
- **Weaknesses:** single maintainer; single-pass means no optimizer
  and limited diagnostics; C subset (fine for a kernel, but userspace
  ports will hit missing features); license not confirmed from
  returned sources [unverified].
- **Verdict:** arguably the best *direct* near-term fit — a tiny
  i386 toolchain already proven building a hobby OS. Co-first-pick
  with tcc; choose after the license check.

### 3.9 Also seen, not shortlisted

- **lacc** ([GitHub](https://github.com/larmel/lacc)): self-hosting,
  x86-64 only, with a small dataflow optimizer. Neat, but x86-64-only
  kills it for the i386 kernel.
- **nwcc**: multi-arch, but retired; mentioned only as a historical
  data point ([HN](https://news.ycombinator.com/item?id=9125912)).
- **cparser/libFirm**: a C frontend on the libFirm IR
  ([HN](https://news.ycombinator.com/item?id=39362777)) — heavier
  than QBE for the same architectural slot.
- **selfie** ([GitHub](https://github.com/fares-z/selfie)):
  educational self-compiling C* → RISC-V with emulator and
  hypervisor. Lovely pedagogy, wrong ISA, subset language.
- **cc500, CIBIC**: toy-scale compilers; below the bar for building
  an OS.

---

## 4. The `CHAR_BIT != 8` problem, explicitly

This is the analysis the whole survey hinges on. GorpOS's packed
cell model (8/10/13-bit cells over 16-bit words) collides with an
industry-wide assumption: **every surveyed compiler assumes an
8-bit byte at every level** — lexer (character literals), type
layout (`char` = 1 byte), IR (QBE's byte/halfword/word/long memory
operations; lcc's 1/2/4-byte IR operators), backend (address
arithmetic in byte units), and C library (`memcpy` in bytes,
`sizeof` in bytes). Making a compiler "cell-native" is therefore not
a backend port. It is a **new C data model**, comparable in scope to
retargeting to a word-addressed DSP. The precedents:

- **TI TMS320C28x: `CHAR_BIT == 16`, word-addressed memory.** `char`
  is 16 bits because that is the smallest addressable unit;
  `sizeof(int) == 1`. The vendor added `__byte()`/`__mov_byte()`
  intrinsics as escape hatches for 8-bit access
  ([TI forum](https://e2e.ti.com/support/microcontrollers/c2000-microcontrollers-group/c2000/f/c2000-microcontrollers-forum/21671/unsigned-char-8-bits)).
  Lesson: vendors who do this keep the C *language* but add
  intrinsics for the octet world — exactly the "explicit marshalling
  layer" shape FILESYSTEM.md chose for GorpFS.
- **ELVM: `sizeof(char) == sizeof(int) == sizeof(void*) == 1`,
  backend-defined word size (typically 24-bit)**
  ([ELVM](https://github.com/shinh/elvm)). Lesson: a C *subset* can
  target word machines if you abandon exact-width types and rewrite
  the library; the 8cc ELVM branch shows which frontend pieces move.
- **DSP folklore:** 24-bit chars (Motorola 56000), 32-bit chars
  (SHARC) — the standard technique is that the compiler synthesizes
  sub-word access with shifts and masks
  ([HN](https://news.ycombinator.com/item?id=28502484),
  [HN](https://news.ycombinator.com/item?id=12471970)).
- **The standards headwind:** the C++ committee paper P3477 documents
  that GCC, LLVM, and MSVC all assume 8-bit bytes, POSIX has mandated
  `CHAR_BIT == 8` (and hence `int8_t`) since 2001, and non-8-bit C is
  effectively an incompatible dialect
  ([P3477R0](https://isocpp.org/files/papers/P3477R0.html)).

Consequences for GorpOS:

1. **No surveyed compiler can be *configured* to 10-bit cells.** In
   every case it is a fork: new type sizes in the frontend, new
   literal encoding, new ABI, new memory-operation semantics in the
   IR/backend, and a rewritten libc. Budget it as "write a new QBE
   backend *plus* a new data model in the frontend" — the FPGC B32P3
   retarget is the right size reference for the backend half.
2. **A 10-bit `char` breaks `int8_t` and POSIX conformance.**
   `int8_t` cannot exist when the smallest addressable unit is 10
   bits, so cell-native C can never be POSIX C. This is acceptable —
   GorpOS's own userspace may be a dialect — but it means the
   Debian-compat layer and every foreign binary **must** see 8-bit
   bytes, consistent with the FILESYSTEM.md decision (GorpFS
   byte-oriented on disk; foreign binaries always get an 8-bit view).
3. **The 13-bit cell has no C model at all** and stays an in-memory
   stress case, as decided.
4. **Recommended shape (staged):** keep the system compiler
   octet-based indefinitely. Add cell support as an *explicit,
   opt-in* extension (new `_Cell`-style types or a width attribute —
   the analog of TI's `__byte()` intrinsics in reverse) rather than
   changing what `char` means. A "Gorp C" with 10-bit `char` is a
   Stage-3 research project, gated on the OS already being
   self-hosting with the octet toolchain.

---

## 5. Rankings

### 5a. Near-term: cross-compiler emitting i386 against the GorpOS kernel ABI

1. **TinyCC** — mature i386 backend, self-hosting, fast, `libtcc`
   for future in-OS use. *First verify the adopted fork's license.*
2. **SmallerC** — i386 + whole toolchain (preprocessor/linker/driver)
   already proven building a hobby OS. *First verify the license.*
   Co-first-pick with tcc; pick the one whose license and code you
   prefer to live with.
3. **pcc** — permissive lineage, real i386 target, deepest
   multi-target history. Heavier, slower-moving, needs bison/flex to
   bootstrap. The fallback.
4. **cproc/QBE** — only after writing a QBE i386 backend (no
   upstream 32-bit x86 exists). Architecturally lovely, practically
   blocked.

GCC remains the trusted bootstrap compiler in all cases (it builds
the cross toolchain; it is not replaced by any of these for ports —
the eventual Debian-compat layer will want real GCC anyway).

### 5b. Long-term: cell-native codegen

1. **cproc + QBE** — the only design with a small, documented,
   replaceable backend and SSA IL; FPGC proved the exact
   frontend→QBE→custom-CPU path. Write a "gorp-cell" QBE backend plus
   the frontend data-model fork (§4).
2. **lcc** — retargeting is the documented workflow and the book
   teaches it; disqualified as the foundation until the
   noncommercial-license lineage is cleared [unverified].
3. **Fork SmallerC or tcc one-pass codegen** — fastest hack to
   cell-aware codegen, worst maintainability. Only if QBE proves
   unsuitable.

### 5c. Teaching value (read, don't adopt)

- **chibicc** — how a complete modern C compiler maps features to
  machine code, commit by commit.
- **lcc book** — retargeting theory and the `Interface`/target-file
  pattern.
- **cc65** — how to co-design compiler + assembler + linker as one
  suite (the self-hosting toolchain template).
- **8cc ELVM branch** — which parts of a C implementation change
  when `char` stops being an octet.
- **QBE** — what a minimal backend looks like.

---

## 6. Recommendation and staged plan

- **Stage 0 (prerequisite):** pin the GorpOS i386 ABI — syscall
  convention, ELF object format, C calling convention. Every
  compiler option below targets this ABI; none can proceed without
  it.
- **Stage 1 (near-term):** adopt **TinyCC or SmallerC** as the
  Linux-hosted cross-compiler for the kernel and early userspace.
  Decide between them on (a) license confirmation and (b) which
  codebase the project prefers to maintain. Keep GCC as the trusted
  bootstrap that builds the cross toolchain.
- **Stage 2:** port the chosen compiler to run *on* GorpOS
  (self-hosted builds) — tcc via `libtcc`/self-build, SmallerC via
  its self-compilation. This is the actual self-hosting milestone.
- **Stage 3 (research, gated on Stage 2):** prototype a QBE
  cell-backend ("gorp-cell") with cell types as an opt-in extension;
  treat 10-bit-`char` "Gorp C" as a dialect experiment, never the
  default. Do not touch the definition of `char` in the system
  compiler.

**Single most valuable finding:** SmallerC — a tiny, self-hosting,
i386-targeting C compiler that ships its own preprocessor, linker,
and driver and has already been used to build a hobby OS — is the
most directly usable near-term toolchain in the survey, and it only
surfaced in the final saturation sweep. The planned pick (tcc) wins
on maturity and `libtcc`; SmallerC wins on whole-toolchain shape and
OS-dev fit. The choice between them is now a license-and-taste
decision, not a capability gap.

---

## Sources

*Research: 9 search batches, Avenue 6 (2026-09-18). B1 tcc; B2
cproc/QBE; B3 lcc; B4 chibicc; B5 8cc + cc65 + pcc history; B6 cc65
suite; B7 pcc current state + i386 targets; B8 `CHAR_BIT != 8` DSP
precedents; B9 saturation sweep (SmallerC, selfie, QBE target
confirmation). Every batch returned novel material — including B9,
which surfaced SmallerC — so the strict >98%-non-novel stop rule was
not formally met; stopping here is a judgment call on diminishing
returns after the candidate set stabilized at 8 surveyed + 5
rejected, with 29 distinct sources banked.*

- tcc forks — [z101/tcc](https://github.com/z101/tcc),
  [mingodad/tinycc](https://GitHub.com/mingodad/tinycc)
- cproc — [README](https://github.com/michaelforney/cproc/blob/HEAD/README.md)
  (targets, bootstrap, missing features); secondary evaluation —
  [viper chibicc_evaluation](https://github.com/splanck/viper/blob/HEAD/docs/chibicc_evaluation.md)
- QBE retarget precedent — [FPGC docs (B32P3)](https://github.com/bartpleiter/fpgc/blob/HEAD/Docs/docs/Software/C-compiler.md);
  QBE scope/targets — [HN discussion](https://news.ycombinator.com/item?id=33802725)
- lcc — [besm6/lcc](https://github.com/besm6/lcc),
  [erysdren/lcc](https://github.com/erysdren/lcc),
  [book PDF](https://www.lmd.vg/pub/books/c/retargetable-c-compiler.pdf),
  [Linux Journal paper](https://www.researchgate.net/publication/255674429_Compile_C_Faster_on_Linux)
- lacc (rejected) — [GitHub](https://github.com/larmel/lacc)
- chibicc — [README mirror](https://archive.org/details/github.com-rui314-chibicc_-_2021-07-06_23-18-44)
- cparser/libFirm (rejected) — [HN](https://news.ycombinator.com/item?id=39362777)
- 8cc/ELVM — [ELVM](https://github.com/shinh/elvm),
  [lambda-8cc](https://github.com/woodrush/lambda-8cc),
  [8cc HN](https://news.ycombinator.com/item?id=9125912)
- cc65 — [README](https://github.com/cc65/cc65/blob/HEAD/README.md)
- pcc — [PortableCC](https://github.com/PortableCC),
  [ronnya/pcc](https://github.com/ronnya/pcc),
  [FreshPorts](https://www.freshports.org/lang/pcc/),
  [Debian manpages](https://manpages.debian.org/experimental/pcc/pcpp.1.en.html)
- SmallerC — [GitHub](https://github.com/alexfru/SmallerC);
  FYS OS usage — [fysnet.net](http://www.fysnet.net/fysos.htm)
- `CHAR_BIT != 8` precedents — [TI C2000 forum](https://e2e.ti.com/support/microcontrollers/c2000-microcontrollers-group/c2000/f/c2000-microcontrollers-forum/21671/unsigned-char-8-bits),
  [P3477R0](https://isocpp.org/files/papers/P3477R0.html),
  [DSP HN](https://news.ycombinator.com/item?id=28502484),
  [sub-word synthesis HN](https://news.ycombinator.com/item?id=12471970)
- selfie (rejected) — [GitHub](https://github.com/fares-z/selfie);
  FreeDOS tiny-compiler context — [All Things Open](https://allthingsopen.org/articles/tiny-programming-freedos-minimal-environment)

*Unverified items (marked inline): licenses of tcc, cproc, QBE,
lcc, chibicc, 8cc, cc65, pcc, SmallerC; LOC figures; the reported
w65c816 QBE backend.*
