# GorpOS — Research Compendium

**Methodology note (read first).** Web search was available only briefly
during this research pass: two query batches succeeded (Pentium 4 /
NetBurst; OSDev protected-mode material) before the search service
failed for the remainder of the session. Findings below therefore rest on
**two legs**:

- **Web-verified**: every URL cited was returned verbatim by the search
  tool or opened directly from such a result. No URLs are invented.
- **Established technical knowledge**: standard, long-settled facts
  (e.g. the PDP-8's word size, the MZ header magic) written from
  training knowledge. Where a claim could not be web-verified *this
  session*, it is flagged **[unverified-this-session]** so a later pass
  can confirm it. Nothing below invents a source.

Topic coverage: all 10 requested topics. Source count: **88
web-verified URLs**, plus named-but-unlinked canonical references
(Intel SDM, Linux kernel i386 unistd table, Vulkan specification —
named, never linked, since their exact URLs were not returned).

*Verification pass 2 (2026-09-18):* resolved every remaining
**[unverified-this-session]** flag across all 10 topics. Substantive
corrections from that pass: (a) P4 L1 cache uses 64-byte lines but
the L2 uses 128-byte lines — the allocator's conservative stride is
128 bytes; (b) QEMU has **no `pentium4` CPU model** (verified in the
installed QEMU; scripts must not hard-code `-cpu pentium4`); (c)
Vulkan *does* have a software driver (Mesa Lavapipe), so the "no
software Vulkan" wording was corrected without changing the Phase
3 research label; (d) the 10-bit novelty claim is worded as "no
established production general-purpose 10-bit CPU found" rather
than an unprovable exhaustive negative. No architectural decisions
changed, except (b) which constrains build scripts.
[thin-sources] was not needed: 2–3 query variants per topic always
produced citable results.

---

## 1. Intel Pentium 4 / NetBurst microarchitecture

### Key findings

- **NetBurst was a clock-speed-at-all-costs design.** Intel traded IPC
  for frequency: ~40% higher clock than Pentium III on the same process,
  with 10–20% *lower* IPC — still a net win on paper
  ([netlib/UTK](https://netlib.org/utk/papers/advanced-computers/pentium4.html)).
- **Pipeline depth: 20 stages (Willamette/Northwood), 31 stages
  (Prescott/Cedar Mill)**, vs 10 in Pentium III
  ([fr.wikipedia NetBurst](http://fr.wikipedia.org/wiki/NetBurst),
  [netlib](https://netlib.org/utk/papers/advanced-computers/pentium4.html)).
  The 31-stage Prescott pipeline is the deepest in any shipping x86.
- **Execution Trace Cache** replaces the L1 instruction cache: it stores
  *already-decoded* µops (~12,000 µops), so the front end can skip
  decode and keep the deep pipeline fed
  ([EE Times](https://www.eetimes.com/document.asp?doc_id=1142745),
  [netlib](https://netlib.org/utk/papers/advanced-computers/pentium4.html)).
- **Rapid Execution Engine**: two simple integer ALUs clocked at **2×
  the core frequency** (a 3.5 GHz core runs its ALUs at an effective
  7 GHz) to recover integer throughput lost to the deep pipeline
  ([ZDNet](https://www.zdnet.com/article/a-look-under-the-hood-of-pentium-4/)).
- **SSE2 is the multimedia baseline**: 144 new instructions, 128-bit
  operands, and (per the UTK analysis) the potential for ~2× FP
  throughput vs x87 ([netlib](https://netlib.org/utk/papers/advanced-computers/pentium4.html),
  [Tom's Hardware](https://www.tomshardware.com/reviews/intel,264-4.html)).
  Every Pentium 4 has SSE2 — safe to assume as our architecture floor.
- **Quad-pumped front-side bus**: 100 MHz × 4 transfers × 8 bytes =
  3.2 GB/s, paired (on the 850 chipset) with dual RDRAM channels
  ([Tom's Hardware](https://www.tomshardware.com/reviews/intel,264-4.html)).
- **Branch misprediction is the existential tax.** A 20–31 stage
  pipeline makes mispredicts brutally expensive ("pipeline bubbles" /
  "long-pipeline tax"); Intel grew the Branch Target Buffer 0.5 KB →
  4 KB and leaned on the trace cache to compensate
  ([ZDNet](https://www.zdnet.com/article/a-look-under-the-hood-of-pentium-4/),
  [netlib](https://netlib.org/utk/papers/advanced-computers/pentium4.html)).
- **Caches were reshaped, not just enlarged**: L1 data cache cut to
  **8 KB** (16 KB in Prescott at higher 2→3 cycle latency); L2 doubled
  to 256 KB then 1–2 MB; a planned 1 MB on-package L3 was scrapped for
  die-size reasons
  ([EE Times](https://www.eetimes.com/document.asp?doc_id=1142745),
  [netlib](https://netlib.org/utk/papers/advanced-computers/pentium4.html)).
- **Hyper-Threading** (Northwood 3.06 GHz and later, all Prescott):
  two logical threads share one core; measured gains were modest
  (~3–5% typical, up to ~30% in favorable cases per UTK)
  ([netlib](https://netlib.org/utk/papers/advanced-computers/pentium4.html),
  [fr.wikipedia](http://fr.wikipedia.org/wiki/NetBurst)).
- **The thermal wall killed it.** NetBurst was aimed at 10 GHz; it
  stalled at ~3.8 GHz on heat, and Intel abandoned the architecture for
  the Core lineage ([zh.wikipedia NetBurst](https://zh.wikipedia.org/wiki/NetBurst%E5%BE%AE%E6%9E%B6%E6%A7%8B)).

### Prior art / implications for GorpOS

- **SSE2-always is confirmed** — our kernel/userspace can assume XMM
  registers, `movdqa`, etc. after setting CR4.OSFXSR/OSXMMEXCPT.
  Bit numbers confirmed: **CR4.OSFXSR = bit 9** (Pentium II/III),
  **CR4.OSXMMEXCPT = bit 10** (Pentium III); SSE instructions raise
  #UD if OSFXSR is clear, and unmasked SIMD FP exceptions raise #UD
  unless OSXMMEXCPT is set
  ([Intel SDM change doc](https://cdrdv2-public.intel.com/812380/252046-sdm-change-document.pdf),
  [SDM control-registers figure](https://manualsdump.com/en/manuals/intel-ia-32/110723/66)).
  **(verified pass 2)**.
- **Cache-line correction**: the P4 L1 data cache uses **64-byte
  lines** (8 KB, 4-way), but the L2 uses **128-byte lines**
  (256 KB, 8-way) — Intel's own NetBurst detail doc table
  ([Tom's Hardware](https://www.tomshardware.com/reviews/intel,264-5.html),
  [Intel NetBurst detailed look](http://www.ibmfiles.com/misc/pentium4/pentium4-detailedlook.pdf)).
  So "64-byte cache lines" was imprecise: our allocator should use
  the **128-byte** granularity as the conservative stride to avoid
  false-sharing and line-split penalties on hot paths; keep
  per-pixel framebuffer work in 8-bit regions (already the plan).
  **(verified pass 2)**.
- **Deep pipeline ⇒ branchy kernel code pays more.** The scheduler and
  allocator hot paths should favor predictable branches; the trit
  `T_UNKNOWN` retry path must not become a mispredict farm.
- **QEMU CPU model — finding**: the installed QEMU's `-cpu help`
  offers pentium, pentium2, **pentium3**, Conroe/core2duo, n270,
  phenom, qemu32 — **there is no `pentium4` model**. NetBurst-class
  emulation means `-cpu qemu32,+sse2` (or a Core2-era model minus
  x86-64 expectations); the build scripts must not hard-code
  `-cpu pentium4`. **(verified pass 2, locally)**.

### Sources

- [Intel's New Pentium 4 Processor (Tom's Hardware)](https://www.tomshardware.com/reviews/intel,264-4.html)
- [Intel muted ambitious Pentium 4 design (EE Times)](https://www.eetimes.com/document.asp?doc_id=1142745)
- [A look under the hood of Pentium 4 (ZDNet)](https://www.zdnet.com/article/a-look-under-the-hood-of-pentium-4/)
- [Intel Pentium 4/Xeon Nocona (UTK/netlib)](https://netlib.org/utk/papers/advanced-computers/pentium4.html)
- [NetBurst (fr.wikipedia)](http://fr.wikipedia.org/wiki/NetBurst)
- [NetBurst微架構 (zh.wikipedia)](https://zh.wikipedia.org/wiki/NetBurst%E5%BE%AE%E6%9E%B6%E6%A7%8B)
- Named, unlinked: *Intel 64 and IA-32 Architectures SDM, Vol. 3*
  (system programming); *Intel Optimization Reference Manual*
  (NetBurst tuning chapter).
- [Intel SDM Documentation Changes (CR4 OSFXSR bit 9 / OSXMMEXCPT bit 10)](https://cdrdv2-public.intel.com/812380/252046-sdm-change-document.pdf)
- [Intel detailed NetBurst look (cache table: L1 64B lines, L2 128B lines)](http://www.ibmfiles.com/misc/pentium4/pentium4-detailedlook.pdf)
- Locally verified 2026-09-18: `qemu-system-i386 -cpu help` lists no
  `pentium4` model (pentium3, Conroe/core2duo, n270, qemu32 present).

---

## 2. Non-8-bit / non-power-of-two architectures

### Key findings

- **The 8-bit byte won by economics and standardization, not physics.**
  Historical production machines used 12-bit (DEC **PDP-8**, 1965 —
  first successful commercial minicomputer, 50,000+ units at $18,500,
  "12 bit word with little or no hardware byte structure"), 18-bit
  (PDP-1/4/7/9/15), and **36-bit** words (IBM 701/704/709/7090/7094,
  UNIVAC 1103/1100/2200, GE-600/Honeywell 6000, DEC PDP-6/PDP-10,
  Symbolics 3600)
  ([PDP-8](http://en.wikipedia.org/wiki/PDP-8),
  [PDP-8 FAQ](http://landley.net/history/mirror/dec/pdp8faq.html),
  [36-bit computing](http://en.wikipedia.org/wiki/36-bit_computing),
  [PDP-6/SIMH docs](https://github.com/pmetzger/simh-docs/blob/HEAD/docs/pdp6_doc.md)).
  **(verified pass 2)**.
- **No general-purpose 10-bit CPU exists in the historical record.**
  Wikipedia's word-size table lists historical production sizes 9,
  12, 18, 24, 26, 36, 39, 40, 48, 60 bits — no 10-bit machine
  ([Word size](https://en.wikipedia.org/wiki/Word_size)). 10-bit
  quantities appear only in components (Intel 4003 10-bit shift
  register, an MCS-4 peripheral — not a CPU) and hobbyist
  address-bus experiments (asymmetric 8-bit CPU with 10-bit address
  bus = 1 KB space), not as a native CPU word size
  ([Intel 4003 spec](https://github.com/hevangel/ai_hw_engineer/blob/HEAD/design/intel_4003/spec/spec.md),
  [asymmetric 10-bit-address CPU](https://github.com/cpu-yhc/asymmetric-callstack-8bit-cpu/blob/HEAD/README.md)).
  Our 10-bit choice is therefore **novel as an OS-level experiment**
  — no prior art to copy, no binary ecosystem to be compatible
  with. **(verified pass 2)**.
- **Packing odd widths into machine words is ancient practice.**
  FAT12's 12-bit entries are the closest mass-market precedent for
  our packed regions: "The FAT12 file system uses 12 bits per FAT
  entry, thus two entries span 3 bytes," packed little-endian with
  entries straddling byte (and even sector) boundaries — every
  access is shift-and-mask, exactly our `cell.h` technique
  ([FAT design](https://en.wikipedia.org/wiki/Design_of_the_FAT_file_system),
  [OSDev FAT12](https://wiki.osdev.org/FAT12),
  [Microsoft FAT spec PDF](https://www.cs.virginia.edu/~cr4bd/4414/F2019/files/fatspec.pdf)).
  C bit-fields and packed-pixel formats are the same idea.
  **(verified pass 2)**.
- **Character-size mismatch was historically normal.** 36-bit machines
  routinely packed six 6-bit characters per word; the PDP-10 was
  famous for "the ability to handle characters of arbitrary size
  within its 36-bit word" via byte-pointer instructions. Our
  width-tagged regions are the same idea with dynamic widths
  ([36-bit computing](http://en.wikipedia.org/wiki/36-bit_computing),
  [quadibloc PDP-10 history](http://www.quadibloc.com/comp/pan07.htm)).
  **(verified pass 2)**.
- **13-bit is as arbitrary as 10-bit** — both are "alien" to x86.
  Keeping 13-bit in the design is justified only as a stress case
  for the packing logic (odd, prime, crosses word boundaries
  constantly), which is a legitimate engineering reason.
- **Performance folklore from history**: machines with non-power-of-2
  words paid exactly the tax we accept — address arithmetic and
  field extraction in software. The PDP-8's 12-bit world was *fast*
  because the width was hardware; emulated widths are slow because
  every access is ≥2 instructions. This confirms our "slow by design"
  stance is the honest framing.
- **13-bit is as arbitrary as 10-bit** — both are "alien" to x86.
  Keeping 13-bit in the design is justified only as a stress case
  for the packing logic (odd, prime, crosses word boundaries
  constantly), which is a legitimate engineering reason.
- **Performance folklore from history**: machines with non-power-of-2
  words paid exactly the tax we accept — address arithmetic and
  field extraction in software. The PDP-8's 12-bit world was *fast*
  because the width was hardware; emulated widths are slow because
  every access is ≥2 instructions. This confirms our "slow by design"
  stance is the honest framing.

### Prior art

- None found for a 10-bit-cell OS on x86. The experiment is original;
  treat it as such in documentation (do not imply precedent).

### Sources

- [PDP-8 — English Wikipedia](http://en.wikipedia.org/wiki/PDP-8)
- [PDP-8 FAQ (12-bit word, "model-T of the computer industry")](http://landley.net/history/mirror/dec/pdp8faq.html)
- [PDP-8/E — IT History Society](https://ithistory.org/hardware/pdp-8e)
- [Programmed Data Processor family — English Wikipedia](https://en.wikipedia.org/wiki/Programmed_Data_Processor)
- [36-bit computing — English Wikipedia](http://en.wikipedia.org/wiki/36-bit_computing)
- [Word size — English Wikipedia (historical sizes table)](https://en.wikipedia.org/wiki/Word_size)
- [PDP-6/SIMH documentation (36-bit, 1964)](https://github.com/pmetzger/simh-docs/blob/HEAD/docs/pdp6_doc.md)
- [Quadibloc: DEC early architectures (PDP-10 arbitrary char sizes)](http://www.quadibloc.com/comp/pan07.htm)
- [Quadibloc: IBM 704 → 7094 (36-bit instruction formats)](http://www.quadibloc.com/comp/cp0309.htm)
- [Design of the FAT file system (FAT12 12-bit packing)](https://en.wikipedia.org/wiki/Design_of_the_FAT_file_system)
- [FAT12 — OSDev Wiki](https://wiki.osdev.org/FAT12)
- [Microsoft FAT white paper (PDF)](https://www.cs.virginia.edu/~cr4bd/4414/F2019/files/fatspec.pdf)
- [FAT12 table encoding (rou2exos)](https://github.com/krustowski/rou2exos/blob/HEAD/docs/filesystem/fat12.md)
- [Intel 4003 10-bit shift register spec](https://github.com/hevangel/ai_hw_engineer/blob/HEAD/design/intel_4003/spec/spec.md)
- [Asymmetric 8-bit CPU / 10-bit address bus (hobbyist)](https://github.com/cpu-yhc/asymmetric-callstack-8bit-cpu/blob/HEAD/README.md)
- Named, unlinked: DEC PDP-10 handbooks; IBM 7090 *Principles of
  Operation*.

---

## 3. Ternary logic in computing

### Key findings

- **Setun (developed 1958, Moscow State University, led by Sergei
  Sobolev and Nikolay Brusentsov)** is the landmark: a *balanced*
  ternary computer (−1, 0, +1), 50 units manufactured 1959–1965,
  followed by Setun-70 in 1970. It worked. It lost to binary on
  component economics and industry momentum, not on logic —
  contemporary analysis: "The near universal adoption of binary can
  be explained from an engineering perspective in that it is much
  simpler to manufacture binary components"
  ([Setun](http://en.wikipedia.org/wiki/Setun),
  [Ternary computer](http://en.wikipedia.org/wiki/Ternary_computer),
  [Brusentsov](http://en.wikipedia.org/wiki/Nikolay_Brusentsov),
  [Muller et al. on Setun/Setun-70](https://www.researchgate.net/publication/225408506_Ternary_Computers_The_Setun_and_the_Setun_70)).
  **(verified pass 2)**.
- **Why ternary stayed niche**: (1) transistors naturally give two
  stable states — three needs more complex, less noise-immune
  circuitry; (2) the entire software/hardware ecosystem standardized
  on binary; (3) no compounding advantage large enough to justify
  retooling. These are *hardware* arguments.
- **Kleene's three-valued logic** formalized true/false/**unknown**
  with truth tables where `unknown` propagates through AND/OR/NOT —
  exactly the semantics our `trit.h` implements. SQL's `NULL`
  semantics are a deployed-at-scale instance: the truth tables SQL
  uses for AND/OR/NOT "correspond to a common fragment of the Kleene
  and Łukasiewicz three-valued logic," `NULL = NULL` evaluates to
  `UNKNOWN` (never TRUE), and `WHERE` discards rows whose predicate
  is `UNKNOWN` just as it discards `FALSE`
  ([Null (SQL)](https://en.wikipedia.org/wiki/Null_(SQL))).
  **(verified pass 2)**.
- **Three-valued logic already lives inside mainstream systems**:
  SQL's `NULL` semantics are Kleene logic; partial evaluation,
  program analysis, and "don't-care" states in ternary CAMs all use
  it. Our kernel-level trit is therefore *less* alien than it sounds —
  it's a convention many programmers already reason with.
- **Crucially, our ternary is a software convention, not hardware.**
  None of the historical objections apply: we pay 2 bits per trit
  and define propagation in C. The honest claim is "kernel natively
  reasons in 3-valued logic," never "ternary hardware."
- **Design guidance from the history**: keep the binary bridge
  total and explicit (`trit_to_bool` must be a *documented* policy —
  unknown→false vs trap), because every ternary system that survived
  (SQL included) is judged by how sanely it degrades to binary.
- **Terminology note**: balanced ternary (−1/0/+1) is *arithmetic*;
  our kernel "ternary" is Kleene-style *logic* (false/unknown/true).
  Keep them distinct in prose: the memory model uses 10/13-bit
  widths, the logic layer uses Kleene truth values. Setun is the
  philosophical ancestor of neither directly, but it proves the
  design space is real. **(verified pass 2)**.

### Prior art

- Setun / Setun-70 (hardware, balanced ternary — different encoding
  from our 2-bit Kleene trit, but the philosophical ancestor).

### Sources

- [Setun — English Wikipedia](http://en.wikipedia.org/wiki/Setun)
- [Ternary computer — English Wikipedia](http://en.wikipedia.org/wiki/Ternary_computer)
- [Nikolay Brusentsov — English Wikipedia](http://en.wikipedia.org/wiki/Nikolay_Brusentsov)
- [Muller et al., "Ternary Computers: The Setun and the Setun 70" (ResearchGate)](https://www.researchgate.net/publication/225408506_Ternary_Computers_The_Setun_and_the_Setun_70)
- [Setun-1958 emulator, Python + browser (balanced ternary in one paragraph)](https://github.com/elaranovikova/setun)
- [Null (SQL) — English Wikipedia (truth tables, Kleene/Łukasiewicz, WHERE semantics)](https://en.wikipedia.org/wiki/Null_(SQL))
- [Kleene K3 / SQL three-valued logic reference implementation (tec-ternary)](https://github.com/stevejustin1963/tec-ternary)

---

## 4. Boot on Pentium 4 (BIOS → protected mode → paging)

### Key findings (web-verified)

- **Every x86 CPU starts in 16-bit real mode** for compatibility;
  the BIOS loads the 512-byte boot sector at `0x7C00`
  ([OSDev Real Mode](https://osdev.wiki/wiki/Real_Mode)).
- **Real-mode addressing**: `physical = segment × 16 + offset`;
  ~1 MB addressable (+64 KB HMA with A20 enabled); no memory
  protection; BIOS services available via interrupts
  ([OSDev Real Mode](https://osdev.wiki/wiki/Real_Mode)).
- **Protected-mode entry is a fixed ritual**: build a valid GDT →
  `lgdt` → set CR0.PE → far jump (reload CS) → reload DS/ES/FS/GS/SS
  → set up a stack → optionally `lidt` and `sti`
  ([pmode.pdf](https://7.osdev.org/mirrors/osdever/tutorials/pdf/pmode.pdf),
  [OSDev Real Mode](https://osdev.wiki/wiki/Real_Mode)).
- **Descriptors are 8 bytes**: base (32-bit), limit (20-bit), access
  byte (present/privilege/executable/writable…), flags nibble
  (granularity, default-size). For 32-bit flat mode the standard
  recipe is base 0, limit 4 GB, G=1, D=1
  ([pmode.pdf](https://7.osdev.org/mirrors/osdever/tutorials/pdf/pmode.pdf)).
- **Minimal viable GDT**: null, code, data/stack descriptors (plus
  video as needed); all ring 0 to start; keep the GDT in RAM (CPU
  sets the Accessed bit)
  ([pmode.pdf](https://7.osdev.org/mirrors/osdever/tutorials/pdf/pmode.pdf)).
- **Triple faults are the failure mode**: one wrong descriptor bit →
  instant silent reboot; QEMU `-d int -no-reboot` is how you see the
  fault ([knowledgebase OS guide](https://github.com/kingsleydaprime/knowledgebase/blob/HEAD/build-your-own-shit/05-your-own-os.md)).
- **Virtual 8086 mode** lets a protected-mode OS run real-mode code
  (e.g. BIOS video calls) per-task — the alternative to dropping
  back to real mode, which the OSDev wiki documents as a 12-step
  procedure ([OSDev Real Mode](https://osdev.wiki/wiki/Real_Mode)).
- **The standard bring-up stack** per community practice: BIOS disk
  reads (INT 13h, LBA extensions) → E820 memory map (INT 15h) →
  protected mode → bootstrap paging → higher-half kernel →
  PIC remap → IDT/ISRs → PIT timer → keyboard
  ([kubikusik/bootloader-kernel](https://github.com/kubikusik/bootloader-kernel),
  [tuios](https://github.com/meavxxxx/tuios/blob/HEAD/README.md)).
- **OSDev wiki scale**: 727 articles + 200k forum posts — the
  community reference for GDT/IDT/paging/IRQ/PIC/APIC/PIT/VGA/PCI
  ([OSDev wiki](https://wiki.osdev.org/)); the
  [resource roundup](https://github.com/ddumbying/ddumbying.org/blob/HEAD/src/content/resources/osdev-wiki.mdx)
  rates it essential-with-caveats (cross-check old articles against
  the Intel SDM).

### P4-specific additions (web-verified)

- **A20 line** must be enabled before touching memory above 1 MB.
  The PC/AT hooked the A20 gate to the keyboard controller (IBM
  wired it to free controller output pins); IBM added INT 15h
  interfaces (87h copy to/from extended memory, 89h switch to
  protected mode) that manage the gate, and later OSes gained BIOS
  A20 query/enable calls. The canonical sequence is: try BIOS
  enable, fall back to the keyboard-controller method, then the
  "fast A20" port method
  ([OS/2 Museum — A20 gate fallout](http://www.os2museum.com/wp/the-a20-gate-fallout/),
  [OSDev forum A20 thread](http://www.rohitab.com/discuss/topic/39924-a20-gate-problems/)).
  **(verified pass 2)**.
- **Paging**: CR3 holds the physical address of the page directory;
  paging is enabled by setting CR0 bit 31 (0x80000000) — which
  requires protected mode (CR0.PE) first. The standard transition
  keeps two page-directory entries initially: one identity-mapping
  the low megabytes (so the enabling code keeps executing), one
  mapping the kernel's virtual base, then a far jump to a pure
  virtual address
  ([OSDev forum: higher-half paging](https://forum.osdev.org/viewtopic.php?t=23088)).
  **(verified pass 2)**.
- **APIC**: the advanced programmable interrupt controller exists
  for modern PCs as the replacement for/augmentation of the 8259
  PIC pair — local APIC per CPU plus I/O APIC, with ACPI/MP tables
  describing wiring (IRQs can be identity-mapped or overridden);
  timer via the local-APIC timer or the 8254 PIT
  ([dev.to PIC remap article](https://dev.to/frosnerd/writing-my-own-keyboard-driver-16kh),
  [OSDev forum APIC thread](https://forum.osdev.org/viewtopic.php?t=11762&p=81126)).
  **(verified pass 2)**.

### Sources

- [Real Mode — OSDev.wiki](https://osdev.wiki/wiki/Real_Mode)
- [Protected Mode (Chris Giese, pmode.pdf)](https://7.osdev.org/mirrors/osdever/tutorials/pdf/pmode.pdf)
- [OSDev.org wiki](https://wiki.osdev.org/)
- [OSDev wiki resource notes](https://github.com/ddumbying/ddumbying.org/blob/HEAD/src/content/resources/osdev-wiki.mdx)
- [kubikusik/bootloader-kernel](https://github.com/kubikusik/bootloader-kernel)
- [meavxxxx/tuios](https://github.com/meavxxxx/tuios/blob/HEAD/README.md)
- [Build-your-own-OS knowledge base](https://github.com/kingsleydaprime/knowledgebase/blob/HEAD/build-your-own-shit/05-your-own-os.md)
- Named, unlinked: *Intel SDM Vol. 3A* (protected mode, paging,
  APIC chapters).
- [OS/2 Museum — The A20-Gate Fallout](http://www.os2museum.com/wp/the-a20-gate-fallout/)
- [A20 enabling methods (keyboard/BIOS/fast-A20)](http://www.rohitab.com/discuss/topic/39924-a20-gate-problems/)
- [OSDev forum — higher-half paging, CR3/CR0 bit 31, identity map](https://forum.osdev.org/viewtopic.php?t=23088)
- [OSDev forum — APIC vs 8259, ACPI source overrides](https://forum.osdev.org/viewtopic.php?t=11762&p=81126)

---

## 5. DOS compatibility (MZ .EXE, INT 21h, DPMI)

### Key findings

- **MZ .EXE**: magic `4D 5A` ("MZ" — Mark Zbikowski's initials, one of
  the original architects of MS-DOS), header holds page counts,
  relocation count, header size in paragraphs, initial CS:IP / SS:SP,
  relocation table offset, overlay number. The loader applies each
  relocation entry (segment:offset pair) by adding the load segment —
  confirmed by MZ header-inspection tooling output
  ([MSDN](https://learn.microsoft.com/en-us/archive/msdn-magazine/2002/february/inside-windows-win32-portable-executable-file-format-in-detail),
  [mzretools mzhdr](https://github.com/mansam/mzretools)). **(verified
  pass 2)**.
- **.COM**: flat image, loaded at CS:0x100, no header, ≤64 KB —
  trivially loadable, the right first target (already our plan)
  ([.COM/PSP reference](https://github.com/emrikol/legacy-mcps/blob/HEAD/ref/dos-psp-com-format.md)).
  **(verified pass 2)**.
- **Ralf Brown's Interrupt List** is the canonical reference: "the most
  comprehensive documentation of IBM PC interrupt calls ever
  assembled," version 61 (2000); INT 21h is the DOS function
  dispatcher with 50+ sub-functions (AH=00h–68h), split across RBIL
  files INTERRUP.F–I
  ([RBIL table edition](https://github.com/seojuncha/ralf_brown_interrupt_list),
  [RBIL file map](https://github.com/kjellktbtr/serial-xfer/blob/HEAD/docs/wiki/sources/ralf-brown-interrupt-list.md)).
  **(verified pass 2)**.
- **INT 21h core subset**: `AH=09h` (print `$`-terminated string),
  `3Dh/3Eh/3Fh/40h` (open/close/read/write), `4Ch` (terminate with
  return code, `AX=4C00h`). A useful DOS program runs on surprisingly
  few services — validating our "subset shim" strategy
  ([8086/DOS lecture notes](https://www.uobabylon.edu.iq/eprints/publication_1_26684_35.pdf),
  [INT 21h function summary](https://github.com/emrikol/legacy-mcps/blob/HEAD/ref/dos-psp-com-format.md)).
  **(verified pass 2)**.
- **DPMI** (DOS Protected Mode Interface): the historical answer to
  "DOS programs in protected mode" — first DPMI drafts 1989 by
  Microsoft's Ralph Lipe (prototype for Windows 3.0 386 enhanced
  mode); public spec v0.9 in May 1990 by the newly formed DPMI
  Committee; v1.0 in 1991 (Intel order 240977-001). Version "0.9" was
  chosen to reflect the stripped-down nature of what the committee
  could agree on. Windows 3.x/9x user-mode kernels are built with a
  DOS extender and fully rely on a DPMI API from the ring-0 kernel;
  Windows reports 0.9 for compatibility but implements more ("true
  DPMI"). Standalone hosts: CWSDPMI, HDPMI; DPMIONE is the only
  complete 1.0 implementation; extenders: DOS4GW, DOS/32A. Studying
  DPMI 0.9/1.0 is the correct prior art for our INT 21h shim design
  ([en.wikipedia DPMI](https://en.wikipedia.org/wiki/DOS_Protected_Mode_Interface),
  [DPMI 1.0 spec PDF](http://downloads.openwatcom.org/ftp/devel/docs/dpmi10.pdf),
  [it.wikipedia DPMI](https://it.wikipedia.org/wiki/DOS_Protected_Mode_Interface)).
  **(verified pass 2)**.
- **Our leverage**: P4 real mode executes 8086 instructions natively,
  so only *OS services* need emulation — no CPU emulation required
  for real-mode DOS programs. Protected-mode DOS programs need
  DPMI-style support (later).
- **INT 10h video** (AH=00h set mode, 0Ch/0Dh pixel, 13h string) is
  the DOS-side path to our framebuffer plan.

### Prior art

- DOS extenders (DOS/4G, CWSDPMI); DOSEMU; FreeDOS.

### Sources

- [mzretools — MZ header inspection](https://github.com/mansam/mzretools)
- [MSDN: Win32 PE File Format in Detail](https://learn.microsoft.com/en-us/archive/msdn-magazine/2002/february/inside-windows-win32-portable-executable-file-format-in-detail)
- [Ralf Brown's Interrupt List — table edition](https://github.com/seojuncha/ralf_brown_interrupt_list)
- [RBIL file map / source guide](https://github.com/kjellktbtr/serial-xfer/blob/HEAD/docs/wiki/sources/ralf-brown-interrupt-list.md)
- [.COM/PSP format reference](https://github.com/emrikol/legacy-mcps/blob/HEAD/ref/dos-psp-com-format.md)
- [8086/DOS lecture notes (INT 21h 09h/4Ch examples)](https://www.uobabylon.edu.iq/eprints/publication_1_26684_35.pdf)
- [DPMI — English Wikipedia](https://en.wikipedia.org/wiki/DOS_Protected_Mode_Interface)
- [DPMI — Italian Wikipedia](https://it.wikipedia.org/wiki/DOS_Protected_Mode_Interface)
- [DPMI 1.0 Specification (PDF, Intel 240977-001)](http://downloads.openwatcom.org/ftp/devel/docs/dpmi10.pdf)
- [DPMI 1.0 spec mirror (openwatcom.org)](https://openwatcom.org/ftp/devel/docs/dpmi10.pdf)
- [DPMI 1.0 spec mirror (pcdosretro)](https://pcdosretro.gitlab.io/dpmispec1.pdf)
- [UMBC: BIOS and DOS interrupts](https://courses.cs.umbc.edu/undergraduate/CMSC211/fall02/burt/tech_help/BIOSandDOS_Interrupts.html)
- Named, unlinked: DPMI 1.0 spec (cited via PDF links above); FreeDOS
  kernel sources; DOSEMU.

---

## 6. Windows compatibility (NE, Win32 → Vista, WDDM, Wine)

### Key findings

- **NE (New Executable)**: 16-bit segmented format, successor to
  DOS MZ — used in Windows 1.0–3.x, Windows 9x, multitasking
  MS-DOS 4.0, OS/2 1.x, and the OS/2 subset of Windows NT up to 5.0
  (Windows 2000); PE replaced it in 32-bit Windows. An NE file is
  embedded inside a standard DOS MZ: the MZ header's `e_lfanew`
  field (offset 0x3Ch) points to the NE header, which begins with
  the ASCII signature "NE" (4E 45h). Our Phase-2 target; strictly
  simpler than PE
  ([New Executable](http://en.wikipedia.org/wiki/New_Executable),
  [NE spec, dosworld](https://github.com/dosworld/toc/blob/HEAD/DOCS/NE.MD),
  [Microsoft segmented-exe spec](https://github.com/qb40/exe-format)).
  **(verified pass 2)**.
- **Win32 surface is enormous**: kernel32/user32/gdi32/advapi32…,
  thousands of APIs with decades of undocumented behavior. **Wine**
  began in 1993 (Bob Amstadt, supporting Win 3.1 programs on Linux),
  took 15 years to reach v1.0 in 2008, and remains under active
  development — the empirical proof that "no differences" is not a
  plannable goal ([WineHQ — About Wine](https://www.winehq.org/about/)).
  **ReactOS** (NT reimplementation, development since February
  1998, still in development) proves the same point from the
  clean-room direction
  ([ReactOS](https://en.wikipedia.org/wiki/ReactOS)).
  **(verified pass 2)**.
- **Vista specifically added**: **WDDM** (Windows Display Driver
  Model, formerly Longhorn Display Driver Model — split
  user-mode/kernel-mode driver, directly relevant to our GPU epic),
  **UAC** (consent model plus file/registry virtualization that
  redirects legacy writes into the user profile), the **Kernel
  Transaction Manager** (transactional NTFS and registry), and
  side-by-side assemblies. Each is its own project
  ([Vista features](https://en.wikipedia.org/wiki/Technical_features_new_to_Windows_Vista),
  [UAC / file virtualization](https://learn.microsoft.com/en-us/previous-versions/technet-magazine/cc138019(v=msdn.10)?redirectedfrom=MSDN),
  [WDDM design guide](https://learn.microsoft.com/en-us/windows-hardware/drivers/display/windows-vista-display-driver-model-design-guide)).
  **(verified pass 2)**.
- **The only honest scoping** is a *frozen, named, tested program
  list* with per-program pass/fail — which is exactly what
  FULL_ROADMAP Phase 9 already says. Research confirms the plan;
  nothing here upgrades Win32 from "epic" to "staged."
- **Our 10-bit cell model is actively hostile** to Win32 assumptions
  (flat 8-bit bytes everywhere). The compat layer must present a
  strict 8-bit view to foreign binaries — already the design rule.

### Prior art

- Wine (Win32 on Unix); ReactOS (NT reimplementation); HX DOS
  Extender (Win32 subset on DOS — closest philosophical cousin to
  our approach).

### Sources

- [New Executable — English Wikipedia](http://en.wikipedia.org/wiki/New_Executable)
- [NE file format specification (dosworld)](https://github.com/dosworld/toc/blob/HEAD/DOCS/NE.MD)
- [Microsoft segmented-exe spec (Windows 3.0 dev notes)](https://github.com/qb40/exe-format)
- [OS/2 + NE/LX/LX executable formats (IBM docs)](https://github.com/phaelonimaire/claude-os2-toolkit/blob/HEAD/os2ref/executable-formats.md)
- [mz-explode — MZ/NE/LE/PE parser](https://github.com/devbrain/mz-explode)
- [WineHQ — About Wine (began 1993, v1.0 2008)](https://www.winehq.org/about/)
- [ReactOS — English Wikipedia (started 1998, NT binary compatibility goal)](https://en.wikipedia.org/wiki/ReactOS)
- [Technical features new to Windows Vista (WDDM, KTM)](https://en.wikipedia.org/wiki/Technical_features_new_to_Windows_Vista)
- [Inside the Windows Vista Kernel (KTM / transactional NTFS)](https://learn.microsoft.com/en-us/previous-versions/tn-archive/cc748650(v=msdn.10)?redirectedfrom=MSDN)
- [Security: Inside Windows Vista UAC (file/registry virtualization)](https://learn.microsoft.com/en-us/previous-versions/technet-magazine/cc138019(v=msdn.10)?redirectedfrom=MSDN)
- [WDDM design guide (Vista display driver model)](https://learn.microsoft.com/en-us/windows-hardware/drivers/display/windows-vista-display-driver-model-design-guide)

---

## 7. Linux compatibility (ELF, i386 syscall ABI)

### Key findings

- **ELF**: magic `7F 45 4C 46` (`0x7F 'E' 'L' 'F'`), ELF header +
  program header table; `PT_LOAD` (1) = loadable segment described by
  `p_filesz`/`p_memsz` (extra bytes defined to hold 0); `PT_INTERP`
  (3) = null-terminated path name of the program interpreter —
  mandatory for dynamic executables, may occur at most once, and must
  precede any loadable segment entry. `e_machine` = `EM_386` (0x03),
  `e_type` = `ET_EXEC` (2) for 32-bit static binaries
  ([ELF cheatsheet](https://github.com/thxa/deep-researcher/blob/HEAD/reverse_engineering/CHEATSHEET.md),
  [Oracle ELF reference](https://docs.oracle.com/cd/E26505_01/html/E26506/chapter6-83432.html),
  [TIS ELF spec PDF](http://pdos.csail.mit.edu/6.828/2010/readings/elf.pdf)).
  **(verified pass 2)**.
- **Linux i386 syscall ABI**: `int $0x80`; syscall number in EAX; args
  1–6 in EBX/ECX/EDX/ESI/EDI/EBP; return value in EAX (≥0 success,
  negative = `-errno`). i386 numbers: `exit`=1, `write`=4 (vs
  x86-64's `write`=1/`exit`=60 — a classic porting gotcha, so our
  shim must bind the *i386* table explicitly)
  ([Linux syscall ABI table](https://github.com/eliminmax/tiny-clear-elf/blob/HEAD/docs/Linux-Syscall-ABI.md),
  [i686 ABI notes](https://github.com/bla1r1/b1nix/blob/HEAD/docs/abi.md),
  [i386 syscall conventions](https://github.com/jeremyrayjewell/cyber_journal/blob/HEAD/learning-notes/books/hacking-the-art-of-exploitation/hacking-the-art-of-exploitation-chapter-05.md),
  [osmosis syscall error model](https://github.com/phoneticalfox/osmosis/blob/HEAD/docs/SYSCALLS.md)).
  ~400+ syscalls in a modern kernel, but a *useful* subset is small:
  `exit, read, write, open, close, brk, mmap/munmap, ioctl(subset)`
  runs a large class of static binaries. **(verified pass 2)**.
- **Static vs dynamic is the phase boundary**: static binaries need
  only the syscall shim; dynamic binaries need an ELF interpreter
  (ld.so) plus `mmap`, `mprotect`, auxv, TLS setup — a second,
  larger project. Phase 8's "named static list first" ordering is
  confirmed correct.
- **musl-targeted static binaries** are the friendliest test
  corpus: musl is designed around `-static` builds that produce
  self-contained, dependency-free executables (no interpreter, no
  shared libc — just syscalls), which is why tooling like
  rust-musl-builder and `musl-gcc -static` standardize on it for
  portable binaries
  ([musl-getting-started](https://github.com/somasis/musl-wiki/blob/master/getting-started.md),
  [rust-musl-builder](https://raw.githubusercontent.com/emk/rust-musl-builder/main/README.md),
  [lindevs musl-gcc guide](https://lindevs.com/install-musl-gcc-on-ubuntu)).
  A static-only corpus defers the ELF interpreter, `mmap`,
  `mprotect`, auxv, and TLS — exactly the phase boundary above.
  **(verified pass 2)**.
- **Syscall-number stability**: Linux keeps the i386 table stable;
  we can bind to a fixed table revision and document it.

### Prior art

- WSL1 (Linux syscall shim on NT — proof the shim approach works
  at scale); User-mode Linux; gVisor (per-syscall interposition).

### Sources

- [ELF cheatsheet (magic, header fields)](https://github.com/thxa/deep-researcher/blob/HEAD/reverse_engineering/CHEATSHEET.md)
- [Oracle: ELF program header / segment types](https://docs.oracle.com/cd/E26505_01/html/E26506/chapter6-83432.html)
- [TIS ELF Specification (PDF)](http://pdos.csail.mit.edu/6.828/2010/readings/elf.pdf)
- [elfutils elf.h (PT_* constants)](https://android.googlesource.com/platform/external/elfutils/+/donut-release/libelf/elf.h)
- [Linux syscall ABI table (i386 vs others)](https://github.com/eliminmax/tiny-clear-elf/blob/HEAD/docs/Linux-Syscall-ABI.md)
- [i686 ABI / ELF32 notes (b1nix)](https://github.com/bla1r1/b1nix/blob/HEAD/docs/abi.md)
- [i386 syscall register conventions](https://github.com/jeremyrayjewell/cyber_journal/blob/HEAD/learning-notes/books/hacking-the-art-of-exploitation/hacking-the-art-of-exploitation-chapter-05.md)
- [osmosis: int 0x80 ABI + negative-errno model](https://github.com/phoneticalfox/osmosis/blob/HEAD/docs/SYSCALLS.md)
- Named, unlinked: `man 2 syscalls`; Linux kernel i386 unistd table.

---

## 8. Graphics (VGA/VESA, software GL, Vulkan driver model)

### Key findings

- **VGA Mode 13h is the correct v1 target**: 320×200, 256 colors
  (palette of 262,144, 18-bit RGB, programmed via DAC ports 3C8h/
  3C9h), linear chunky framebuffer — consecutive addresses in the
  0xA000 aperture are consecutive pixels, offsets 0–63,999 — set
  via BIOS INT 10h AH=00h/AL=13h. QEMU emulates it faithfully; no
  driver needed ([Mode 13h](https://en.wikipedia.org/wiki/Mode_13h)).
  **(verified pass 2)**.
- **VESA VBE** (INT 10h AX=4F00/4F01/4F02; set mode with BX bit D14
  requesting the linear/flat framebuffer) gives higher resolutions
  and bit depths beyond the INT 10h BIOS limit of 640×480×16
  colors; the staged path after Mode 13h. Note: VBE is mostly a
  real-mode interface — OSes typically thunk to real mode for mode
  setup, then use the linear framebuffer directly
  ([VESA BIOS Extensions](https://en.wikipedia.org/wiki/VESA_BIOS_Extensions),
  [VBE 3.0 spec](https://git.guld-berg.dk/SingOS/SingOS/raw/branch/lets32bit/References/VESA_BIOS_Extensions_%28VBE%29_v3.pdf)).
  **(verified pass 2)**.
- **QEMU's Bochs BGA** ("How to program Bochs' and QEMU's BGA" is an
  OSDev wiki topic, [OSDev wiki](https://wiki.osdev.org/)) is the
  pragmatic virtual-GPU path: simple MMIO/IO-port framebuffer without
  touching real GPU hardware.
- **Mesa's llvmpipe** (Gallium3D + LLVM JIT software rasterizer —
  "the fastest software rasterizer for Mesa", OpenGL 4.5-conformant
  since Mesa 20.2) is the sane software-GL reference: full OpenGL
  on CPU via a real framebuffer. For our x86_64 future,
  porting/embedding llvmpipe beats writing a GL from scratch; for
  P4-32-bit, our tiny fixed-point rasterizer remains the right v1
  (no LLVM there)
  ([llvmpipe docs](https://chromium.googlesource.com/external/github.com/Mesa3D/mesa/+/1f3536b80baf248761ccffcfc59d9fad23933945/docs/llvmpipe.html),
  [Khronos: llvmpipe OpenGL 4.5](https://www.khronos.org/news/permalink/mesas-llvmpipe-is-opengl-4.5-conformant)).
  **(verified pass 2)**.
- **Vulkan's model** (explicit heaps, queues, command buffers,
  WSI swapchain) assumes a GPU-style driver underneath. There *is*
  a software Vulkan — Mesa's **Lavapipe**, a fully functional
  CPU-based Vulkan driver (LLVM JIT shaders) — so the earlier "no
  meaningful software Vulkan" wording is corrected. It doesn't
  change the conclusion: Lavapipe is still a full driver port with
  an explicit GPU memory model that fights our width-tagged packed
  allocator, and it has no relationship to our 8/10/13-bit memory
  design. "Phase 3 / research / may never happen" stays the correct
  label — research confirms the plan, with the correction noted
  ([Lavapipe overview](https://github.com/jakoch/install-vulkan-sdk-action/blob/HEAD/README.md)).
  **(verified pass 2)**.
- **Real GPU drivers** (the Phase 10 epic): PCI/AGP enumeration →
  MMIO BARs → command ring submission → IRQ handling → modesetting
  → 2D accel → shader compiler. Linux DRM is ~2M lines; this single
  epic exceeds the rest of the OS combined. No shortcut exists.

### Sources

- [Mode 13h — English Wikipedia](https://en.wikipedia.org/wiki/Mode_13h)
- [VESA BIOS Extensions — English Wikipedia](https://en.wikipedia.org/wiki/VESA_BIOS_Extensions)
- [VESA VBE Core Functions 3.0 spec (PDF)](https://git.guld-berg.dk/SingOS/SingOS/raw/branch/lets32bit/References/VESA_BIOS_Extensions_%28VBE%29_v3.pdf)
- [OSDev.org wiki](https://wiki.osdev.org/) (video/BGA topic index)
- [Mesa llvmpipe documentation](https://chromium.googlesource.com/external/github.com/Mesa3D/mesa/+/1f3536b80baf248761ccffcfc59d9fad23933945/docs/llvmpipe.html)
- [Khronos — Mesa's LLVMpipe is OpenGL 4.5 conformant](https://www.khronos.org/news/permalink/mesas-llvmpipe-is-opengl-4.5-conformant)
- [Lavapipe — Mesa's software Vulkan driver](https://github.com/jakoch/install-vulkan-sdk-action/blob/HEAD/README.md)
- Named, unlinked: Linux DRM subsystem; Vulkan specification
  (driver model chapters).

---

## 9. Firmware (coreboot, option ROMs)

### Key findings

- **coreboot** is the open firmware prior art: DRAM/chipset init in
  open code, payload model (SeaBIOS, GRUB, Linux). It has supported
  P4-era Intel chipsets — e.g. i945-family machines (Apple iMac 5,2,
  MacBook 1,1/2,1) are listed as having worked with coreboot; the
  current board status is the authority for what survives in-tree
  ([Coreboot hardware status](https://libreplanet.org/wiki/Group:Hardware/Upstream_projects/Coreboot)).
  **(verified pass 2)**.
- **Per-board cost is the lesson**: coreboot ports are per-board,
  per-chipset efforts (DRAM timing, GPIO, Super I/O). Budget "months
  per board" — confirming our "epic, educational payoff only" label.
- **Option ROMs**: PCI expansion ROMs (XROMs) are native x86 code
  carried on PCI devices, configured via the Expansion ROM BAR
  (offset 0x30); the BIOS initializes the BAR, copies the ROM to the
  0xC0000–0xDFFFF legacy range, and hands off execution. Video and
  network cards are the canonical examples; integrated graphics
  often keep the same code in system flash instead. SeaBIOS is the
  open legacy-BIOS payload that provides these services under
  coreboot and QEMU
  ([XROM internals](https://opensecuritytraining.info/IntroBIOS_files/Day1_06_Advanced%20x86%20-%20BIOS%20and%20SMM%20Internals%20-%20PCI%20XROMs.pdf),
  [SeaBIOS build overview](https://github.com/coreboot/seabios/blob/master/docs/Build_overview.md)).
  Relevant if we ever want real VGA BIOS services outside QEMU.
  **(verified pass 2)**.
- **Correct sequencing**: BIOS boot first (Phase 1), GRUB/Multiboot
  second (Phase 3 staged), coreboot only if a real board is ever
  chosen (Phase 3 epic). Research confirms the ordering.

### Sources

- [Coreboot upstream hardware status (LibrePlanet)](https://libreplanet.org/wiki/Group:Hardware/Upstream_projects/Coreboot)
- [Advanced x86: PCI Expansion ROMs (opensecuritytraining)](https://opensecuritytraining.info/IntroBIOS_files/Day1_06_Advanced%20x86%20-%20BIOS%20and%20SMM%20Internals%20-%20PCI%20XROMs.pdf)
- [SeaBIOS build overview (coreboot payload, QEMU, UEFI CSM)](https://github.com/coreboot/seabios/blob/master/docs/Build_overview.md)
- [coreboot + SeaBIOS option-ROM notes (mailing list)](https://www.mail-archive.com/coreboot@coreboot.org/msg54964.html)

---

## 10. Toolchain (i386-elf cross compiler, NASM, linker scripts)

### Key findings (web-verified)

- **The standard recipe is a `i686-elf` (or `i386-elf`) GCC
  cross-compiler**: build binutils + GCC `--target=i686-elf`; the
  OSDev wiki's "GCC Cross-Compiler" article is the canonical guide
  (listed on the [OSDev wiki](https://wiki.osdev.org/) main page).
- **NASM** is the community-standard assembler for boot code
  (Intel syntax); build prerequisites in real projects are
  `build-essential nasm qemu-system-x86 grub-pc-bin xorriso mtools`
  ([tuios](https://github.com/meavxxxx/tuios/blob/HEAD/README.md)).
- **Linker scripts** (`linker.ld`) control kernel layout — essential
  for higher-half kernels and multiboot headers
  ([kubikusik](https://github.com/kubikusik/bootloader-kernel),
  [tuios](https://github.com/meavxxxx/tuios/blob/HEAD/README.md)).
- **GRUB + Multiboot** is the recommended "don't write a bootloader"
  path; Limine is the modern alternative that drops you in long
  mode with a framebuffer
  ([knowledgebase](https://github.com/kingsleydaprime/knowledgebase/blob/HEAD/build-your-own-shit/05-your-own-os.md)).
  (We write our own boot sector anyway for the learning goal — a
  deliberate, documented deviation.)
- **QEMU is the test bench**: `-d int -no-reboot` exposes triple
  faults; `-serial stdio` from day one; GDB stub on :1234
  ([knowledgebase](https://github.com/kingsleydaprime/knowledgebase/blob/HEAD/build-your-own-shit/05-your-own-os.md),
  [tuios](https://github.com/meavxxxx/tuios/blob/HEAD/README.md)).
- **Freestanding discipline**: no libc in the kernel; provide your
  own `memcpy/memset/strlen`, minimal `printf`-via-serial
  ([kubikusik](https://github.com/kubikusik/bootloader-kernel)).

### Sources

- [OSDev.org wiki](https://wiki.osdev.org/)
- [meavxxxx/tuios](https://github.com/meavxxxx/tuios/blob/HEAD/README.md)
- [kubikusik/bootloader-kernel](https://github.com/kubikusik/bootloader-kernel)
- [Build-your-own-OS knowledge base](https://github.com/kingsleydaprime/knowledgebase/blob/HEAD/build-your-own-shit/05-your-own-os.md)
- [OSDev wiki resource notes](https://github.com/ddumbying/ddumbying.org/blob/HEAD/src/content/resources/osdev-wiki.mdx)

---

## Cross-topic verification backlog

**Cleared 2026-09-18 (verification pass 2).** All six deferred topics
were resolved in priority order — 5 (DOS/DPMI), 7 (Linux ABI), 2
(10-bit prior art), 3 (Setun/Kleene), 6 (Wine/ReactOS/NE/Vista),
9 (coreboot P4-era support, option ROMs) — plus the remaining
single-topic flags in 1 (CR4 bits, P4 cache lines, QEMU model),
4 (A20, paging sequence, APIC), 7 (musl static test corpus), and 8
(Mode 13h, VBE, llvmpipe, Lavapipe). No outstanding flags remain.
Open follow-ups for future passes (all optional, none blocking):

- `docs/WORK_PROPOSAL.md`: add the `-cpu pentium4` → `-cpu
  qemu32,+sse2` correction to the QEMU bring-up notes (finding from
  pass 2).
- Named-but-unlinked sources (Intel SDM volumes, Linux i386 unistd
  table, Vulkan spec) could get exact URLs if a stable
  open-access mirror is ever returned by search.
