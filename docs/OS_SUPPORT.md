# GorpOS — OS Compatibility Support Research (Linux & Unix, ~2007 era)

**Method:** Survey Linux distributions and Unix systems adjacent to
Debian through the Vista era (~2007), anchored on **Debian 4.0 "Etch"**
(April 2007, Linux 2.6.18, 11 architectures) as the compatibility
baseline. For each OS: lineage, kernel, executable format, syscall/libc
distinctions, and the compatibility delta *beyond* the Debian shim (which
already handles Linux/ELF/glibc-on-i386).

**Assessment values:**
- `covered-by-Debian-shim` — same Linux syscall ABI + ELF + glibc
  family; Debian-derived distros. Binaries "just run."
- `small-delta` — still Linux/ELF; only glibc-version behavior, distro
  library versions, or packaging differ. A version-matrix note, not a
  new ABI.
- `large-delta` — different syscall table and/or different binary
  format (ELF branding, XCOFF, Mach-O, a.out); needs a per-OS ABI
  vector like FreeBSD's Linuxulator, plus loader and libc emulation.
- `research` — feasibility unknown or the OS model is alien (Hurd,
  Plan 9).

**The core insight:** ELF is a *format*, not an ABI. Solaris, IRIX,
Linux, and the BSDs all use ELF with incompatible syscall tables,
branding, dynamic linkers, signal behavior, and libcs. Binary
compatibility = format + syscall ABI + libc + loader, and the format is
the easy part. FreeBSD's Linuxulator is the design precedent: ELF
branding selects an alternate syscall table per process.

**Verification note:** version/year details compiled from general
references, individually `[unverified]` unless cited. Format/syscall
claims carry citations. Research: 6 batches / 17 searches; stopped per
the >98%-non-novel rule.

---

## Linux distributions (Debian-adjacent)

| OS (era version) | Lineage | Kernel | Binary format | libc / syscall notes | Compat delta vs Debian shim | Assessment |
|---|---|---|---|---|---|---|
| Debian GNU/Linux 4.0 "Etch" (Apr 2007) | Debian | Linux 2.6.18 | ELF | glibc 2.3.6 | None — this is the baseline. | covered-by-Debian-shim |
| Ubuntu 7.04 "Feisty" (Apr 2007) | Debian-derived | Linux 2.6.20 | ELF | glibc (newer than Etch) | Library/package-version behavior only; same syscall ABI. | covered-by-Debian-shim |
| Kubuntu / Xubuntu / Edubuntu 7.04 | Ubuntu flavors | Linux 2.6.20 | ELF | same as Ubuntu | Same as Ubuntu. | covered-by-Debian-shim |
| Knoppix 5.1/5.2 (2007) | Debian-derived live CD | Linux 2.6.19 | ELF | glibc | Same as Debian. | covered-by-Debian-shim |
| MEPIS 6.5 (2007) | Debian/Ubuntu-based | Linux 2.6.15 | ELF | glibc | Same as Debian. | covered-by-Debian-shim |
| Linspire 5 / Freespire 2.0 (2007) | Debian-based | Linux 2.6.x | ELF | glibc | Same as Debian. | covered-by-Debian-shim |
| Xandros 4.x (2006–07) | Debian-based | Linux 2.6.x | ELF | glibc | Same as Debian. | covered-by-Debian-shim |
| Damn Small Linux 3.4/4.x (2007) | Knoppix/Debian-based | Linux 2.4/2.6 | ELF | glibc/uClibc mix | 2.4-kernel-era syscalls on some builds; note only. | covered-by-Debian-shim |
| Fedora 7 "Moonshine" (May 2007) | Red Hat lineage | Linux 2.6.21 | ELF | glibc 2.6 | Same Linux syscall ABI; glibc-version behavior notes. | small-delta |
| RHEL 5 / CentOS 5 (2007) | Red Hat lineage | Linux 2.6.18 | ELF | glibc 2.5 | Same ABI; older glibc than Fedora 7. | small-delta |
| openSUSE 10.3 (Oct 2007) | SUSE lineage | Linux 2.6.22 | ELF | glibc 2.6.1 | Same ABI. | small-delta |
| SLES/SLED 10 SP1 (2007) | SUSE lineage | Linux 2.6.16 | ELF | glibc 2.4 | Same ABI; enterprise library versions. | small-delta |
| Slackware 12.0 (Jul 2007) | Independent | Linux 2.6.21 | ELF | glibc 2.5 | Same ABI; no dependency resolution affects our shim not at all. | small-delta |
| Gentoo 2007.0 | Source-based | user-chosen 2.6.x | ELF | glibc (user-built) | Same ABI; compile flags vary, irrelevant to binaries. | small-delta |
| Arch Linux 0.8 "Voodoo" (Apr 2007) | Independent | Linux 2.6.20 | ELF | glibc 2.5 | Same ABI. | small-delta |
| Mandriva 2007.1/2008 | Mandrake/Red Hat lineage | Linux 2.6.17/22 | ELF | glibc | Same ABI. | small-delta |
| Puppy Linux 2.17/3.00 (2007) | Independent | Linux 2.6.18/21 | ELF | glibc | Same ABI; tiny userland. | small-delta |
| BackTrack 2/3 (2007–08) | Slackware-based | Linux 2.6.20 | ELF | glibc | Same ABI. | small-delta |
| Debian GNU/kFreeBSD (tech preview, 2007) | Debian userland + FreeBSD kernel | FreeBSD 6.x | ELF | **GNU libc on a BSD kernel** | FreeBSD syscall table under glibc — needs the BSD ABI vector *and* glibc port. Both deltas at once. | large-delta |
| Debian GNU/Hurd (K16, 2007) | Debian + GNU Mach/Hurd | GNU Mach | ELF | glibc on Hurd translators | No Unix syscall table; completely different IPC model. | research |

Debian Etch (Apr 8 2007, 2.6.18, 11 arches):
[Wikipedia/Debian version history](https://en.wikipedia.org/wiki/Debian_version_history).
Ubuntu 7.04 (Apr 19 2007, 2.6.20):
[Launchpad/Feisty](https://launchpad.net/ubuntu/feisty).

## Unix systems

| OS (era version) | Lineage | Kernel | Binary format | libc / syscall notes | Compat delta vs Debian shim | Assessment |
|---|---|---|---|---|---|---|
| FreeBSD 6.2/7.0 (2007–08) | BSD | FreeBSD | ELF (branded "FreeBSD") | BSD libc; different syscall numbers | Needs BSD syscall table + signal/loader fixups. Linuxulator is the exact design precedent (ELF branding → alternate ABI vector per process). | large-delta |
| NetBSD 3.1/4.0 (2006–07) | BSD | NetBSD | ELF | BSD libc | Same class as FreeBSD; NetBSD's own compat_linux exists. | large-delta |
| OpenBSD 4.1/4.2 (2007) | BSD | OpenBSD | ELF (also a.out historically) | BSD libc | Same class; OpenBSD removed Linux compat in 5.x — era-correct target only. | large-delta |
| DragonFly BSD 1.8/1.10 (2007) | BSD fork | DragonFly | ELF | BSD libc | Same class. | large-delta |
| Solaris 10 (Jan 2005; 6/06 update) | SVR4 | SunOS 5.10 | ELF | Solaris libc; SVR4 syscalls | ELF but different syscall table, branding, dynamic linker (`ld.so.1`), signal semantics. lxrun-style Linux compat existed via BrandZ (later). | large-delta |
| OpenSolaris (2005+, SXDE 2007) | SVR4/open | SunOS 5.11 | ELF | same as Solaris 10 | Same as Solaris 10. | large-delta |
| AIX 5L 5.3 / 6.1 (2007) | IBM (SVR-ish) | AIX (POWER) | **XCOFF** (not ELF) | AIX libc; POWER BE | Needs XCOFF loader + runtime linker + AIX syscall shim + BE audit. The only XCOFF system on the list. | large-delta |
| HP-UX 11i v2/v3 (2004/2007) | HP (SVR-ish) | HP-UX (PA-RISC/Itanium) | ELF (PA-RISC, Itanium) | HP-UX libc | Different syscall table; PA-RISC is BE. | large-delta |
| IRIX 6.5 (MIPS) | SGI SVR4 | IRIX | ELF (MIPS, o32/n32/n64 ABIs!) | IRIX libc | ELF but three MIPS ABIs (o32/n32/n64) — the loader must disambiguate. GA ended Dec 29 2006 — inside our era boundary. | large-delta |
| Tru64 UNIX 5.1B (Alpha) | DEC OSF/1 | OSF/1 (Mach-derived) | ELF (Alpha) | DEC libc | Different syscall table; LE like x86 (easiest Unix port after the BSDs). | large-delta |
| macOS 10.4 Tiger / 10.5 Leopard | Darwin (Mach + BSD) | XNU | **Mach-O** (fat binaries) | libSystem; BSD-ish syscalls *under* Mach IPC | Mach-O loader + Mach trap handling; BSD layer is close-ish but Mach IPC is the hard part. | large-delta |
| SCO OpenServer 6 / UnixWare 7.1.4 | SCO SVR5 | SVR5 | ELF | SCO libc | SVR4-ish but its own syscall table; tiny market — lowest priority. | large-delta |
| QNX Neutrino 6.3 (2005) | QNX microkernel | Neutrino | ELF | QNX libc; **message-passing, no Unix syscall table** | Needs a syscall-personality layer over Send/Receive/Reply; architecturally the hardest "Unix." | large-delta |
| MINIX 3.1.x (2005–07) | Tanenbaum microkernel | MINIX | a.out → ELF (transition era) | MINIX libc; tiny POSIX-ish syscall set | Different format *and* microkernel IPC; teaching OS — low priority. | large-delta |
| BeOS R5 (2000) / Haiku (2002+) | Be | BeOS/Haiku | ELF on x86 (R4+); **PEF on PPC** | BeOS API (not POSIX-first) | Not a Unix syscall model at all; Be API emulation is a research project. | research |
| Plan 9, 4th ed. (2002) | Bell Labs | Plan 9 | a.out (8a/8c/8l) | lib9; **9P + per-process namespaces, no Unix syscalls** | Alien model; research-only. | research |

ELF ≠ ABI (Solaris/IRIX/Linux/BSD all ELF, all incompatible):
[maskray: evolution of ELF](https://maskray.me/blog/2024-05-26-evolution-of-elf-object-file-format).
FreeBSD ELF branding + Linuxulator design:
[FreeBSD handbook: binary formats](https://docs-archive.freebsd.org/doc/7.4-RELEASE/usr/share/doc/handbook/binary-formats.html),
[Linuxulator advanced](https://docs-archive.freebsd.org/doc/10.3-RELEASE/usr/local/share/doc/freebsd/en/books/handbook/linuxemu-advanced.html).
Solaris 10 (Jan 2005):
[The Register](https://www.theregister.com/2011/06/28/oracle_solaris_11_old_iron_mia).
IRIX GA ended Dec 29 2006:
[ZDNet](https://www.zdnet.com/article/sgis-unix-variant-fading-into-history/).
AIX XCOFF (not ELF):
[IBM AIX files reference](http://ibm.com/docs/en/ssw_aix_72/filesreference/filesreference_pdf.pdf).
BeOS x86 R4+ uses ELF; PPC uses PEF:
[maskray: evolution of ELF](https://maskray.me/blog/2024-05-26-evolution-of-elf-object-file-format).

## Analysis

**Distribution:** covered-by-Debian-shim: 8 · small-delta: 10 ·
large-delta: 15 · research: 3 (36 total).

**Decision-relevant findings:**

1. **The entire Linux world is one syscall ABI.** All 18 Linux rows are
   covered or small-delta. The Debian shim's real work is glibc-version
   behavior and distro library paths — not new ABIs. Say this loudly in
   FULL_ROADMAP: Linux compat is a *version matrix*, not a porting
   program.
2. **Every Unix is a large-delta, and each needs its own ABI vector.**
   FreeBSD's Linuxulator proves the design: per-process syscall tables
   selected by ELF branding. Our compat layer should be built as
   `src/compat/<os>/` vectors from day one, not as Debian-shim
   special cases.
3. **AIX is the format outlier** — the only XCOFF system. An XCOFF
   loader is a self-contained epic (documented by IBM, at least). It
   also forces the BE audit, shared with the PPC/SPARC Tier-2 CPU ports.
4. **IRIX's three MIPS ABIs (o32/n32/n64) are a loader-design warning:**
   our ELF loader must key off `e_flags`/EI_CLASS, not assume one ABI
   per OS. Same lesson applies to Solaris (32/64-bit) and macOS (fat
   binaries).
5. **Debian GNU/kFreeBSD is the fascinating edge case:** GNU libc on a
   BSD kernel means *both* deltas at once. It's the test that proves
   the ABI-vector design is correctly factored. Keep it as a design
   spike, not a milestone.
6. **QNX and Plan 9 bound the problem:** QNX shows what "Unix-like but
   message-passing" costs; Plan 9 shows where Unix compatibility ends
   entirely. Both are correctly `large-delta`/`research`, not roadmap
   items.

## Sources

- Per-OS versions: Wikipedia family/version pages, vendor docs
  (individually `[unverified]` unless cited above).
- Cited URLs listed inline above (Debian version history, Launchpad,
  maskray ELF, FreeBSD handbook ×2, The Register, ZDNet, IBM AIX docs).

*Research log: 6 batches / 17 searches total for this task; batches 5–6
confirmed known facts (Etch/Feisty dates, ELF≠ABI, XCOFF, IRIX EOL) rather
than novel material; stopped per the >98%-non-novel rule.*
