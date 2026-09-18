# GorpOS — Filesystem: FAT Reference + GorpFS Design

**Status:** research + design sketch. No implementation in this pass.
**Why FAT first:** two independent consumers need it — the stage-2 boot
loader must read the kernel image off disk (WORK_PROPOSAL Phase 3
staged: "FAT/ext2 second-stage reader"), and the DOS-compat layer
(FULL_SCOPE Layer 6) needs to read DOS floppies/disk images. FAT is
the *interchange* format; our own volumes get a native filesystem
(GorpFS) later. Read FAT for compat, write GorpFS for ourselves.

**Method note:** FAT facts below are either web-verified (cited) or
long-settled established knowledge marked as such. The canonical
reference is Microsoft's `fatgen103` ("FAT: General Overview of
On-Disk Format"); the most stable mirror returned by search is the
University of Virginia copy.

---

## Part 1 — FAT reference

### 1.1 Volume layout (all FAT types)

```
Sector 0 .. RsvdSecCnt-1        Reserved area (boot sector + extras)
RsvdSecCnt ..                   FAT region (NumFATs copies, usually 2)
FAT region end ..               Root directory (FAT12/16: fixed-size region)
Data area                       Clusters numbered from 2 upward
```

Key formulas (established; cf. the Stanford cs140e FAT32 lab notes):

```
FATStart      = RsvdSecCnt                        (sectors, from volume start)
RootDirStart  = FATStart + NumFATs * FATSz         (FAT12/16 only)
DataStart     = RootDirStart + RootDirSectors      (FAT12/16)
              = FATStart + NumFATs * FATSz32       (FAT32; root dir is a cluster chain)
FirstSectorOfCluster(N) = DataStart + (N - 2) * SecPerClus
```

Cluster count determines the FAT *type* (established):
`< 4085` → FAT12 · `< 65525` → FAT16 · otherwise FAT32. The type is
*derived from the geometry*, not trusted from the `FilSysType` string.

### 1.2 Boot sector / BPB field table

All offsets are bytes from sector start; all multi-byte integers are
**little-endian**. Offsets 0x0B–0x23 are the common BPB.

| Offset | Size | Name | Meaning |
|--------|------|------|---------|
| 0x00 | 3 | `BS_jmpBoot` | `EB ?? 90` or `E9 ?? ??`; jump to bootstrap code |
| 0x03 | 8 | `BS_OEMName` | OEM string, conventionally `"MSWIN4.1"` (compat hint, not trusted) |
| 0x0B | 2 | `BPB_BytsPerSec` | Bytes per sector: 512, 1024, 2048, 4096 (use 512 for compat) |
| 0x0D | 1 | `BPB_SecPerClus` | Sectors per cluster: power of 2, 1–128 |
| 0x0E | 2 | `BPB_RsvdSecCnt` | Reserved sectors before first FAT (incl. boot sector) |
| 0x10 | 1 | `BPB_NumFATs` | Number of FAT copies (usually 2) |
| 0x11 | 2 | `BPB_RootEntCnt` | Max 32-byte root-dir entries (FAT12/16; 0 for FAT32) |
| 0x13 | 2 | `BPB_TotSec16` | Total sectors if < 65536, else 0 |
| 0x15 | 1 | `BPB_Media` | Media descriptor (`0xF8` = hard disk, `0xF0` = 1.44 MB floppy) |
| 0x16 | 2 | `BPB_FATSz16` | Sectors per FAT (FAT12/16; 0 for FAT32) |
| 0x18 | 2 | `BPB_SecPerTrk` | Sectors per track (geometry hint) |
| 0x1A | 2 | `BPB_NumHeads` | Number of heads (geometry hint) |
| 0x1C | 4 | `BPB_HiddSec` | Hidden sectors before volume (partition offset) |
| 0x20 | 4 | `BPB_TotSec32` | Total sectors (used when TotSec16 == 0) |

**FAT12/16 extended boot record** (offset 0x24):

| Offset | Size | Name | Meaning |
|--------|------|------|---------|
| 0x24 | 1 | `BS_DrvNum` | INT 13h drive number (`0x80` = first hard disk) |
| 0x25 | 1 | `BS_Reserved1` | Reserved (0) |
| 0x26 | 1 | `BS_BootSig` | Extended boot signature, `0x29` |
| 0x27 | 4 | `BS_VolID` | Volume serial number |
| 0x2B | 11 | `BS_VolLab` | Volume label, space-padded |
| 0x36 | 8 | `BS_FilSysType` | `"FAT12   "` / `"FAT16   "` (informational only) |
| 0x1FE | 2 | — | Boot signature `0x55 0xAA` |

**FAT32 extended BPB** (replaces the FAT12/16 extension at 0x24):

| Offset | Size | Name | Meaning |
|--------|------|------|---------|
| 0x24 | 4 | `BPB_FATSz32` | Sectors per FAT |
| 0x28 | 2 | `BPB_ExtFlags` | Bit 7: 0 = mirrored FATs, 1 = single active FAT (bits 0–3 = active FAT#) |
| 0x2A | 2 | `BPB_FSVer` | Filesystem version (0x0000) |
| 0x2C | 4 | `BPB_RootClus` | First cluster of root directory (usually 2) |
| 0x30 | 2 | `BPB_FSInfo` | Sector number of FSInfo (usually 1) |
| 0x32 | 2 | `BPB_BkBootSec` | Backup boot sector location (usually **6** — see §1.5) |
| 0x34 | 12 | `BPB_Reserved` | Must be zero |
| 0x40 | 1 | `BS_DrvNum` | Drive number |
| 0x41 | 1 | `BS_Reserved1` | Reserved |
| 0x42 | 1 | `BS_BootSig` | `0x29` |
| 0x43 | 4 | `BS_VolID` | Volume serial |
| 0x47 | 11 | `BS_VolLab` | Volume label |
| 0x52 | 8 | `BS_FilSysType` | `"FAT32   "` (informational) |
| 0x1FE | 2 | — | `0x55 0xAA` |

Sources: Microsoft fatgen103 spec
([fatspec.pdf](https://www.cs.virginia.edu/~cr4bd/4414/F2019/files/fatspec.pdf));
FAT32 extended fields
([flylib](https://flylib.com/books/en/3.210.1.65/1/),
[VLSI docs](https://www.vlsi.fi/player_vs1011_1002_1003/modularplayer/structDiskBlock_1_1Fat_1_1Extensions_1_1Fat32Specific.html));
retrocmp boot-sector table
([retrocmp.de](https://retrocmp.de/fdd/general/bootsector.pdf)).

### 1.3 The FAT: table structure and cluster chains

The FAT is an array with **one entry per cluster** (clusters 0 and 1
are reserved; usable clusters start at 2). Each entry is either the
number of the *next* cluster in the file's chain, or a special value.

**FAT12 — the packed precedent.** Entries are 12 bits; **two entries
span 3 bytes**, packed little-endian: reading the 3 bytes as one
LE 24-bit value, the low 12 bits are the first entry and the high 12
bits are the second
([fatspec.pdf](https://www.cs.virginia.edu/~cr4bd/4414/F2019/files/fatspec.pdf),
[fatgen103 mirror](https://8dcc.github.io/external/fatgen103.pdf)).
The spec's canonical access pattern (cluster `N`, byte offset
`ThisFATEntOffset = N + N/2`):

```
entry = *(WORD*)&SecBuff[ThisFATEntOffset];
if (N is odd)  entry >>= 4;        // high 12 bits
else           entry &= 0x0FFF;    // low 12 bits
```

This is *exactly* our `cell.h` shift-and-mask technique, deployed at
planetary scale since 1980 — the closest mass-market precedent for
packed odd-width storage (RESEARCH §2).

**FAT16:** one 16-bit LE entry per cluster. **FAT32:** one 32-bit LE
entry per cluster, but only the **low 28 bits** are the cluster
number; the high 4 bits are reserved (mask with `0x0FFFFFFF`).

**Entry values** (FAT-type-dependent widths; `?` = 0 for FAT12,
F for FAT16, 0F for FAT32):

| Value | Meaning |
|-------|---------|
| `0x?000` | Free cluster |
| `0x?FF0`–`0x?FF6` | Reserved |
| `0x?FF7` | **Bad cluster** (never allocate; never make this an allocatable cluster number on FAT32) |
| `0x?FF8`–`0x?FFF` | **End of clusterchain (EOC)** — this cluster is the file's last. Microsoft drivers *write* `0x?FFF` (`0x0FFFFFFF` on FAT32) |
| anything else | Next cluster number in the chain |

**FAT[0] and FAT[1] (reserved clusters):**
- `FAT[0]` = `BPB_Media` byte in the low 8 bits, all other bits set
  to 1 (e.g. `0x0FF8` / `0xFFF8` / `0x0FFFFFF8` for media `0xF8`).
- `FAT[1]` = EOC mark. On FAT16/32 the **high 2 bits** of FAT[1]
  are the dirty-volume flags: `ClnShutBitMask` (`0x8000` FAT16,
  `0x08000000` FAT32) = 1 means cleanly unmounted; `HrdErrBitMask`
  (`0x4000` / `0x04000000`) = 1 means no I/O errors last mount.
  Clear-on-mount, set-on-clean-unmount — the poor man's journal.

Sources:
[fatspec.pdf](https://www.cs.virginia.edu/~cr4bd/4414/F2019/files/fatspec.pdf),
[cs140e FAT32 notes](https://raw.githubusercontent.com/dddrrreee/cs140e-20win/6c73b422bc904fafa6e216a7c58f5c8dc5c12737/labs/13-fat32/docs/pauls-fat32.annoted.pdf),
[Stanford CS111 lecture](https://web.stanford.edu/class/archive/cs/cs111/cs111.1252/lectures/2/Lecture2.pdf).

### 1.4 Directory entries (32 bytes each)

Short (8.3) entry layout:

| Offset | Size | Name | Meaning |
|--------|------|------|---------|
| 0x00 | 11 | `DIR_Name` | 8.3 name, space-padded, uppercase ASCII |
| 0x0B | 1 | `DIR_Attr` | Attribute bits (below) |
| 0x0C | 1 | `DIR_NTRes` | NT reserved / lowercase flags |
| 0x0D | 1 | `DIR_CrtTimeTenth` | Creation time, tenths of a second |
| 0x0E | 2 | `DIR_CrtTime` | Creation time |
| 0x10 | 2 | `DIR_CrtDate` | Creation date |
| 0x12 | 2 | `DIR_LstAccDate` | Last access date |
| 0x14 | 2 | `DIR_FstClusHI` | High 16 bits of first cluster (FAT32; 0 on FAT12/16) |
| 0x16 | 2 | `DIR_WrtTime` | Last write time (**required**) |
| 0x18 | 2 | `DIR_WrtDate` | Last write date (**required**) |
| 0x1A | 2 | `DIR_FstClusLO` | Low 16 bits of first cluster |
| 0x1C | 4 | `DIR_FileSize` | File size in bytes (0 for directories) |

**Attribute bits:** `0x01` read-only · `0x02` hidden · `0x04` system ·
`0x08` volume label · `0x10` directory · `0x20` archive.
**Name byte 0:** `0x00` = entry free *and* no allocated entries follow
in this directory (scan stop); `0xE5` = deleted (free, keep scanning);
`0x05` = literal `0xE5` (Japanese kanji escape hatch).
Directories contain `.` and `..` entries (except the FAT12/16 root);
`..` in the root's child points at cluster 0.

**FAT date/time formats** (16-bit each, established; creation/access
fields optional, write fields required):

```
Date: bits 15–9 = years since 1980 (0–127 → 1980–2107)
      bits 8–5  = month (1–12)
      bits 4–0  = day (1–31)
Time: bits 15–11 = hours (0–23)
      bits 10–5  = minutes (0–59)
      bits 4–0   = seconds/2 (0–29 → 0–58 s, 2-second granularity)
```

Sources:
[fatspec.pdf](https://www.cs.virginia.edu/~cr4bd/4414/F2019/files/fatspec.pdf)
(date/time formats, FAT[0]/[1] semantics),
[UMass 2017](https://people.cs.umass.edu/~liberato/courses/2017-spring-compsci365/lecture-notes/11-fats-and-directory-entries/)
and [UMass 2019](https://people.cs.umass.edu/~liberato/courses/2019-spring-compsci590f/lecture-notes/09-fat-and-ntfs/)
lecture notes (entry layout, LFN).

### 1.5 Long filenames (LFN)

LFN entries are ordinary 32-byte directory entries with
`DIR_Attr == 0x0F` (`READ_ONLY|HIDDEN|SYSTEM|VOLUME_ID`) — a
combination old DOS ignores (especially the volume-label bit). They
**precede** the short entry they belong to, in **reverse** order.
Each holds **13 UCS-2 characters**; `FstClusLO` must be 0.

| Offset | Size | Name | Meaning |
|--------|------|------|---------|
| 0x00 | 1 | `LDIR_Ord` | Sequence number, 1-based; **last** entry ORed with `0x40` |
| 0x01 | 10 | `LDIR_Name1` | Characters 1–5 (UCS-2) |
| 0x0B | 1 | `LDIR_Attr` | Must be `0x0F` |
| 0x0C | 1 | `LDIR_Type` | 0 = long-name subcomponent |
| 0x0D | 1 | `LDIR_Chksum` | Checksum of the short 8.3 name |
| 0x0E | 12 | `LDIR_Name2` | Characters 6–11 |
| 0x1A | 2 | `LDIR_FstClusLO` | Must be 0 |
| 0x1B | 4 | `LDIR_Name3` | Characters 12–13 |

The checksum guards against LFN/SFN mismatch after edits by
non-LFN-aware tools (orphaned LFN entries are treated as free).
Canonical checksum over the 11 short-name bytes:

```
Sum = 0
for i in 0..10:
    Sum = ((Sum & 1) ? 0x80 : 0) + (Sum >> 1) + Name[i]
```

Sources:
[fatspec.pdf](https://www.cs.virginia.edu/~cr4bd/4414/F2019/files/fatspec.pdf),
[Linux vfat.txt](https://kernel.googlesource.com/pub/scm/linux/kernel/git/jejb/linux-coc/+/refs/tags/v4.8-rc8/Documentation/filesystems/vfat.txt),
[Old New Thing on LFN design](https://devblogs.microsoft.com/oldnewthing/20110826-00/?p=9793).

### 1.6 FAT32 extras: FSInfo and the backup boot sector

- `BPB_FSInfo` (0x30) → sector number of the **FSInfo sector**
  (usually 1). FSInfo structure:
  - `FSI_LeadSig` @0 (4) = `0x41615252`
  - `FSI_Reserved1` @4 (480) = zeros
  - `FSI_StrucSig` @484 (4) = `0x61417272`
  - `FSI_Free_Count` @488 (4) = last known free cluster count
    (`0xFFFFFFFF` = unknown, recompute)
  - `FSI_Nxt_Free` @492 (4) = hint where to start scanning for free
    clusters (`0xFFFFFFFF` = start at cluster 2)
  - `FSI_Reserved2` @496 (12) = zeros
  - `FSI_TrailSig` @508 (4) = `0xAA550000` (last two bytes match
    the `0x55 0xAA` boot signature)
- `BPB_BkBootSec` (0x32) → **backup boot sector** (usually 6).
  Microsoft's "boot sector" is actually **three** sectors (boot,
  FSInfo, plus one more), and all three are duplicated at the
  backup location (so backup FSInfo usually lands at sector 7).
  The value 6 is effectively hard-wired in repair tools — never use
  any other value.

Sources:
[fatspec.pdf](https://www.cs.virginia.edu/~cr4bd/4414/F2019/files/fatspec.pdf),
[TechNet FAT32 changes](https://learn.microsoft.com/en-us/previous-versions/cc768180(v=technet.10)).

### 1.7 Minimal FAT reader checklist (for our stage-2 loader)

For a read-only FAT12/16/32 reader (boot + DOS compat), these fields
suffice (cf. the cs140e "four variables" reduction):

1. Validate `0x55AA` @0x1FE.
2. `BytsPerSec` @0x0B (expect 512), `SecPerClus` @0x0D,
   `RsvdSecCnt` @0x0E, `NumFATs` @0x10.
3. FAT size: `FATSz16` @0x16 or `FATSz32` @0x24.
4. Compute `FATStart`, `DataStart`, `FirstSectorOfCluster` (§1.1).
5. FAT12/16: root dir = fixed region after FATs
   (`RootEntCnt` @0x11 × 32 bytes). FAT32: root = cluster chain
   from `RootClus` @0x2C.
6. Walk cluster chains with the §1.3 entry reader; EOC test is
   `>= 0x0FF8 / 0xFFF8 / 0x0FFFFFF8`.
7. Parse 32-byte dir entries (§1.4); skip LFN entries (`attr ==
   0x0F`) on first pass, then use them for long names.

Typical FAT16 cluster sizes (established): 256–511 MB → 8 KB;
512 MB–1 GB → 16 KB; 1–2 GB → 32 KB. FAT16 caps at ~2–4 GB.

---

## Part 2 — The cell-model tension analysis

This is the design question that makes our filesystem interesting:
**every on-disk format in existence is byte-oriented, and our memory
model is not.**

### 2.1 Where the tension actually lives

The disk itself is byte-addressable (512-byte sectors of octets).
The CPU is byte-addressable (CHAR_BIT == 8 on every target we will
ever support — CPU_SUPPORT.md's hard line). The *only* place
8/10/13-bit cells exist is **our in-memory representation**. So the
tension is not "how do we store 10-bit cells on a byte disk" — that
is trivially packing — but **where the cell↔octet boundary sits in
the software stack, and what invariants it must preserve**:

1. **Foreign binaries must see a strict 8-bit view** (WORK_PROPOSAL
   D6; FULL_SCOPE Layer 6). Any file handed to a DOS/Win32/Linux
   program is an octet stream, full stop.
2. **The kernel's cell layer is an allocator and access discipline**,
   not a storage format. Regions are width-tagged *in memory*.
3. **On-disk metadata must be parseable by plain C structs.**
   If the superblock required a 10-bit cell reader to parse, we
   could never bootstrap: the stage-2 loader (real-mode, 16-bit)
   has to read it before the cell layer exists.

Conclusion: **GorpFS is a byte-oriented on-disk format by
construction.** The cell model influences *what hints we store*
(width preferences) and *how the VFS materializes data in memory*,
never the encoding of the bytes on the platter.

### 2.2 Historical precedent: the byte was always a convention

Two precedents show this is a solved philosophical problem:

- **PDP-10 byte pointers.** The PDP-10 had `LDB`/`DPB` (load/deposit
  byte) instructions driven by a byte pointer with explicit **P**
  (position) and **S** (size) fields — "the ability to handle
  characters of arbitrary size within its 36-bit word"
  ([RESEARCH §2](RESEARCH.md#2-non-8-bit--non-power-of-two-architectures),
  [PDP-10 reference](https://archive.org/download/bitsavers_decpdp10KAstemReferenceManual196805_7730036/DEC-10-HGAA-D_PDP-10_System_Reference_Manual_196805.pdf)).
  TOPS-10 filesystems stored 5×7-bit, 4×9-bit, or other packings;
  the *file* was a sequence of software-defined bytes, and the disk
  format just stored the underlying words. Our width tags are the
  same idea: the byte quantum is a parameter, not a law. (Amusing
  footnote: a 30-bit word divides evenly into **10-bit characters**
  — [quadibloc's table](http://www.quadibloc.com/arch/ar0201.htm).
  Our 10-bit cell has at least one historical cousin.)
- **FAT12's 12-bit entries** (§1.3): the mass-market proof that
  odd-width packing into octets is routine. The packing *is* the
  format; no one calls FAT12 "a 12-bit filesystem."

So: the disk stores octets; the OS defines what a "cell" means when
it materializes those octets into memory. The boundary is a
**marshalling layer**, and it needs exactly one canonical spec.

### 2.3 The marshalling contract (normative for GorpFS)

1. **Files are octet streams on disk. Always.** A file's bytes on
   the platter are the bytes a foreign program reads. No implicit
   width anywhere in file data.
2. **Cell-native blobs are packed with a declared width.** If a
   program wants to store 10-bit cells (e.g. a packed pixel buffer
   that will be unpacked by the cell layer), it stores them as a
   packed bit-stream and declares the width in file metadata
   (extended attribute / inode hint). The kernel's cell layer does
   the unpacking on read — the same job `cell.h` already does.
3. **Canonical bit order (normative):** bit-stream packing is
   **LSB-first within each byte, bytes in little-endian order** —
   cell *i* occupies stream bits `[i·w, i·w + w)`, with stream bit 0
   = bit 0 of byte 0. This is exactly the FAT12 convention ("the 12
   lsbits are the first entry") and matches our little-endian
   platform. Any packed structure on disk (bitmaps included) uses
   this order.
4. **On-disk metadata is fixed-width LE integers and byte arrays
   only.** No packed cells in the superblock, inodes, or directory
   entries. The stage-2 loader and a host-side `mkfs` must parse
   them with plain C — this is a bootstrap requirement, not a
   preference.
5. **The CHAR_BIT wall does not reach the disk.** CPU_SUPPORT.md
   establishes CHAR_BIT != 8 as a type-system blocker for the
   *kernel*. The filesystem is insulated: every target parses the
   same octet format. If the long-term self-hosting compiler
   (docs/COMPILERS.md) ever targets a 10-bit-char machine, the VFS
   boundary still speaks octets; the *backend* does the marshalling.
   The FS never learns about it.

### 2.4 Where the cell model *does* shape the design

- **Inode `width_hint`:** each inode records the preferred
  in-memory cell width for its data (8/10/13, default 8). The VFS
  page/region cache allocates a region of that width when
  materializing the file. This is a *performance and correctness*
  hint for cell-native programs, not a format change: the bytes on
  disk are identical either way.
- **Allocator symmetry:** our in-memory allocator is first-fit over
  width-tagged regions; the on-disk allocator should be simple and
  auditable rather than clever — bitmap first-fit, with a
  "next-free hint" in the superblock (the FSInfo `FSI_Nxt_Free`
  idea). Two allocators, two jobs; don't unify them.
- **13-bit data:** the prime-width stress case stays in memory. On
  disk it is just bits in the canonical packing. Nothing special.
- **The real trap — implicit width in shared code:** a VFS helper
  that assumes "1 cell == 1 byte" (e.g. `memcpy`-shaped copy loops
  over "cells") will silently corrupt 10/13-bit data. Mitigation:
  **distinct types for cell streams vs octet streams** at the VFS
  boundary, with conversion only at explicit marshall points
  (`octets_to_cells` / `cells_to_octets`). This is the same lesson
  as WORK_PROPOSAL D5's explicit binary bridge for trits.
- **Endianness audit:** LE integers on disk are unambiguous, but
  *bit order inside packed streams is not fixed by byte order*.
  The canonical bit order (§2.3.3) must be documented in the
  superblock spec and tested by `mkfs`/`fsck` round-trips — this is
  the BE-port audit flag from CPU_SUPPORT.md applied to our own
  format.

### 2.5 What we deliberately do NOT do

- No 10-bit or 13-bit sectors, no "cell-addressed disk." The block
  device speaks 512-byte sectors; LBA is in sectors.
- No width-tagged *directories* or *inodes* — fixed 8-bit metadata.
- No journaling in v0 (ext2 shipped without one; we document the
  crash-consistency gap honestly and stage the fix).
- No LFN-style contortions: long names are native (ext2-style
  variable-length entries), because we control the format.

---

## Part 3 — GorpFS v0 design sketch

**Goals:** minimal viable native filesystem — mountable, with
`mkfs`/`fsck` host-testable; simple enough to implement after the
FAT reader; honest about what it lacks (crash consistency, extents).

**Fixed parameters:** 512-byte sectors · block = 8 sectors (4 KiB) ·
little-endian · magic `"GORPFS01"`.

### 3.1 On-disk layout

```
Sector 0                  Superblock (primary)      [512 B]
Sector 1 .. S             Boot/code area (reserved, optional stage-2 payload)
Block B_sb2               Superblock (backup copy)  [1 block]
Block B_bmap              Block bitmap              [1 bit per block]
Block B_imap              Inode bitmap              [1 bit per inode]
Block B_itab ..           Inode table               [128-byte inodes]
...                       Data blocks
```

Text diagram (4 KiB blocks, `N` data blocks, `I` inodes):

```
+--------+--------+--------+--------+--------+--------//--------+
| SB pri | boot   | SB bak | blkbmp | inobmp | itable | data...  |
| blk 0  | blk 1  | blk 2  | blk 3  | blk 4  | blk 5+ |          |
+--------+--------+--------+--------+--------+--------//--------+
  \________ superblock fields point at each region's block number;
             nothing is at a hardcoded offset except sector 0 __/
```

Backup superblock placement mirrors the FAT32 lesson (§1.6): a
second copy at a *fixed, documented* location (block 2, and
optionally a third at the last block for large volumes) so a repair
tool can find it without parsing anything.

### 3.2 Superblock (sector 0, 512 bytes)

| Offset | Size | Name | Meaning |
|--------|------|------|---------|
| 0x00 | 8 | `magic` | `"GORPFS01"` |
| 0x08 | 2 | `version` | `0x0001` |
| 0x0A | 2 | `sectors_per_block` | 8 (block = 4 KiB) |
| 0x0C | 4 | `block_count` | Total blocks in volume |
| 0x10 | 4 | `inode_count` | Total inodes |
| 0x14 | 4 | `block_bitmap` | Block # of block bitmap |
| 0x18 | 4 | `inode_bitmap` | Block # of inode bitmap |
| 0x1C | 4 | `inode_table` | First block # of inode table |
| 0x20 | 4 | `data_start` | First data block # |
| 0x24 | 4 | `root_inode` | Inode # of root directory (1) |
| 0x28 | 4 | `free_blocks` | Last known free block count (`0xFFFFFFFF` = unknown) |
| 0x2C | 4 | `free_inodes` | Last known free inode count |
| 0x30 | 4 | `next_free_block` | Allocation hint (FSInfo-style; `0xFFFFFFFF` = scan from `data_start`) |
| 0x34 | 2 | `state` | `1` = clean, `2` = dirty (mounted / not cleanly unmounted — the FAT[1] dirty-flag idea) |
| 0x36 | 2 | `features` | Feature flags (bit 0: has backup SB; reserved otherwise) |
| 0x38 | 4 | `mkfs_version` | Tool version that created the volume |
| 0x3C | 16 | `uuid` | Volume UUID |
| 0x4C | 16 | `label` | Volume label, UTF-8, NUL-padded |
| 0x5C | 4 | `checksum` | CRC32 (or simple sum in v0) over bytes 0x00–0x5B |
| 0x60 | ~350 | `reserved` | Zero |
| 0x1FE | 2 | — | `0x55 0xAA` (firmware politeness; not a FAT claim) |

Inode numbering: **1-based** (inode 1 = root), mirroring ext2's
"inode 2 is root" convention but starting at 1; inode 0 = "none".

### 3.3 Inode (128 bytes)

| Offset | Size | Name | Meaning |
|--------|------|------|---------|
| 0x00 | 2 | `mode` | Type + permissions (Unix-style bits) |
| 0x02 | 2 | `nlink` | Hard link count |
| 0x04 | 2 | `uid` | Owner |
| 0x06 | 2 | `gid` | Group |
| 0x08 | 2 | `flags` | Reserved |
| 0x0A | 8 | `size` | File size in **octets** |
| 0x12 | 8 | `atime` | Access time, Unix epoch seconds |
| 0x1A | 8 | `mtime` | Modification time |
| 0x22 | 8 | `ctime` | Status-change time |
| 0x2A | 1 | `width_hint` | Preferred in-memory cell width: 8 (default), 10, 13; 0 = 8 |
| 0x2B | 1 | `ftype` | 0 = unknown, 1 = regular, 2 = directory, 3 = symlink |
| 0x2C | 4 | `reserved` | Zero |
| 0x30 | 40 | `direct[10]` | 10 direct block numbers (u32 each) |
| 0x58 | 4 | `indirect` | Single-indirect block # |
| 0x5C | 4 | `dindirect` | Double-indirect block # |
| 0x60 | 32 | `reserved2` | Zero (extension room: xattr block pointer goes here in v1) |

Block pointers are **u32 block numbers**; block 0 = "hole"
(Minix convention — unallocated). 10 direct + single + double
indirect covers tiny-to-large without the triple-indirect
complexity (ext2 uses 12+1+1+1; Minix v1 used 7+1+1 — our 10+1+1
sits between; the inode table design is borrowed from Minix/ext2:
[Minix FS intro](https://tenox.pdp-11.net/os/minix/ibmpc/Introduction%20to%20the%20Minix%20File%20System.pdf),
[ext2 design](http://web.mit.edu/tytso/www/linux/ext2intro.html)).

Timestamps are **Unix epoch u64**, not FAT's packed 16-bit format —
we are not DOS and the 2107/2-second-granularity limits buy us
nothing.

### 3.4 Directories: variable-length entries

ext2-style (no 8.3, no LFN hack — long names are native):

```
+----------+--------+----------+--------+---------------+
| ino (u32)| reclen | namelen  | ftype  | name bytes... |
|  4 B     | (u16)  |  (u8)    | (u8)   |  namelen B    |
+----------+--------+----------+--------+---------------+
```

- `reclen` = total entry length including padding to a 4-byte
  boundary; entries pack densely, deleted entries are coalesced
  (reclen absorbs the gap).
- `ino == 0` marks a free slot (scan continues — unlike FAT's
  `0x00` stop byte, which only works because FAT never compacts).
- Name: UTF-8, ASCII subset in v0; max 255 bytes.
- `.` and `..` entries present in every directory except root
  (same convention as FAT/Unix).

### 3.5 Allocation: bitmap + hint

- **Block bitmap:** 1 bit per block, canonical LSB-first bit order
  (§2.3.3). Block *n* ↔ bit `(n mod 8)` of byte `(n div 8)`.
- **Inode bitmap:** same scheme for inodes.
- **Allocation policy:** first-fit from `next_free_block`
  (wrap-around scan), mirroring FAT32's `FSI_Nxt_Free` hint. Update
  the hint on every alloc/free; recompute by scan if it ever points
  at an allocated block. Simple, auditable, O(n) worst case — fine
  for v0 volumes (QEMU disk images, not data centers).
- **Free counts** in the superblock are hints (`0xFFFFFFFF` =
  unknown), exactly like `FSI_Free_Count`.

### 3.6 Cell data on disk (the marshalling layer, concretely)

- Regular file data = octets. If `width_hint != 8`, the VFS
  allocates an in-memory region of that width and unpacks the
  octet stream through `octets_to_cells()` using the canonical bit
  order. `size` is always in octets — the on-disk truth.
- A cell-native tool that wants to *store* packed 10/13-bit data
  writes the packed bit-stream itself (via the cell layer's
  `cells_to_octets()`) and sets `width_hint` so readers unpack
  symmetrically. Round-trip property: 
  `octets_to_cells(cells_to_octets(x)) == x` — tested by
  `mkfs`/`fsck` on host.
- Bitmaps, being kernel metadata, use the same canonical order so
  host tools and kernel agree bit-for-bit.

### 3.7 What v0 omits (staged, honestly)

| Omission | Staged fix |
|----------|-----------|
| Crash consistency (torn metadata writes) | Dirty flag + ordered writes first; then atomic metadata updates — littlefs's **metadata pairs** (two blocks + revision count, alternating updates so a backup always exists) are the design to steal: [littlefs DESIGN.md](https://github.com/littlefs-project/littlefs/blob/master/DESIGN.md) |
| No extents (block chains via indirect blocks only) | Fine for v0 sizes; extents are a v1 optimization |
| No hardlink dirs, no xattrs, no ACLs | `reserved2` in the inode is the extension room |
| Single block size (4 KiB) | Parameter exists (`sectors_per_block`); only 8 is tested in v0 |
| fsck is offline-only | Online scrubbing is research |

---

## Part 4 — Migration path

| Stage | Work | Reads | Writes |
|-------|------|-------|--------|
| **S1 — boot** (Phase 3 staged) | Read-only FAT12/16 in the stage-2 loader; kernel image + config live on a FAT volume | FAT12/16 | — |
| **S2 — interchange** | Kernel VFS + FAT12/16/32 driver (read/write); DOS floppy images, USB sticks | FAT12/16/32 | FAT12/16/32 |
| **S3 — native volumes** | `mkfs.gorp` on host; kernel mounts GorpFS v0 for system volumes (`/`, `/boot`); FAT remains the foreign-interchange FS | GorpFS v0 | GorpFS v0 |
| **S4 — robustness** (staged) | Dirty-flag discipline, ordered metadata writes, offline `fsck.gorp`; evaluate metadata-pair atomic updates | GorpFS v0 | GorpFS v0+ |
| **S5 — research** | Cell-aware extents? Content-defined chunking for the packed regions? Only after S3 is boring | — | — |

**What never happens:** GorpFS does not replace FAT as the DOS
interchange format (D6/D7: compat is shim-based, and the world's
DOS/Windows tooling speaks FAT). GorpFS does not try to be ext4;
if we ever need a *serious* Unix filesystem, porting ext2 read
support (spec: [ext2 design](http://web.mit.edu/tytso/www/linux/ext2intro.html))
is cheaper than growing one.

**Boot-sector coexistence note:** GorpFS sector 0 carries our magic
at offset 0, *not* a FAT BPB — a FAT driver must not mistake it for
FAT. The `0x55AA` signature at 0x1FE keeps firmware happy without
claiming FAT-ness. `mkfs.gorp` refuses to format a volume with a
valid FAT signature unless `--force` is given (same for the reverse).

---

## Sources

- Microsoft fatgen103, "FAT: General Overview of On-Disk Format" —
  [fatspec.pdf (UVa mirror)](https://www.cs.virginia.edu/~cr4bd/4414/F2019/files/fatspec.pdf)
  ([raw.githubusercontent mirror](https://raw.githubusercontent.com/yxu1183/FAT-32/2d1ccc72c80e7dc8393f05639f8923c017cb8b0f/fatspec.pdf));
  FAT12 access pseudocode also at
  [8dcc fatgen103 mirror](https://8dcc.github.io/external/fatgen103.pdf)
- FAT32 FSInfo/backup boot sector —
  [TechNet](https://learn.microsoft.com/en-us/previous-versions/cc768180(v=technet.10)),
  [flylib field table](https://flylib.com/books/en/3.210.1.65/1/),
  [fatgen103 mirror (scribd)](https://www.scribd.com/document/857216584/Microsoft-Extensible-Firmware-Initiative-FAT32-File-System-Specification-Version-1-03-20001206),
  [VLSI FAT32 BPB docs](https://www.vlsi.fi/player_vs1011_1002_1003/modularplayer/structDiskBlock_1_1Fat_1_1Extensions_1_1Fat32Specific.html)
- Boot-sector/BPB field tables —
  [retrocmp.de](https://retrocmp.de/fdd/general/bootsector.pdf),
  [networkintelligence.ai](https://networkintelligence.ai/blogs/volume-boot-sector-format-of-fat/)
- Directory entries + LFN —
  [UMass COMPSCI 365 (2017)](https://people.cs.umass.edu/~liberato/courses/2017-spring-compsci365/lecture-notes/11-fats-and-directory-entries/),
  [UMass COMPSCI 590F (2019)](https://people.cs.umass.edu/~liberato/courses/2019-spring-compsci590f/lecture-notes/09-fat-and-ntfs/),
  [Linux vfat.txt](https://kernel.googlesource.com/pub/scm/linux/kernel/git/jejb/linux-coc/+/refs/tags/v4.8-rc8/Documentation/filesystems/vfat.txt),
  [Old New Thing: LFN design notes](https://devblogs.microsoft.com/oldnewthing/20110826-00/?p=9793)
- FAT32 geometry/reader reduction —
  [cs140e FAT32 lab notes](https://raw.githubusercontent.com/dddrrreee/cs140e-20win/6c73b422bc904fafa6e216a7c58f5c8dc5c12737/labs/13-fat32/docs/pauls-fat32.annoted.pdf),
  [Stanford CS111 lecture 2](https://web.stanford.edu/class/archive/cs/cs111/cs111.1252/lectures/2/Lecture2.pdf)
- Inode/bitmap design (Minix, ext2) —
  [Minix FS intro](https://tenox.pdp-11.net/os/minix/ibmpc/Introduction%20to%20the%20Minix%20File%20System.pdf),
  [ext2 design (Tytso)](http://web.mit.edu/tytso/www/linux/ext2intro.html),
  [CalPoly FS lectures](https://users.csc.calpoly.edu/~pnico/class/now/cpe453/notes/lecture_24.pdf),
  [Stanford CS140 adv_fs](https://www.scs.stanford.edu/15au-cs140/notes/adv_fs.pdf),
  [Silberschatz ch.11 slides](http://cc.ee.ntu.edu.tw/~farn/courses/OS/slides/ch11.pdf),
  [TUHS filesystems lecture](https://minnie.tuhs.org/CompArch/Lectures/week11.html)
- Atomic metadata design (littlefs) —
  [littlefs DESIGN.md](https://github.com/littlefs-project/littlefs/blob/master/DESIGN.md)
- Non-8-bit precedent (PDP-10 byte pointers, word/character tables) —
  [PDP-10 System Reference Manual (bitsavers)](https://archive.org/download/bitsavers_decpdp10KAstemReferenceManual196805_7730036/DEC-10-HGAA-D_PDP-10_System_Reference_Manual_196805.pdf),
  [quadibloc word/character table](http://www.quadibloc.com/arch/ar0201.htm)

*Research: 12 search batches, Avenue 5 (2026-09-17/18). Batches 1–7
each returned novel material (BPB tables, FAT32 FSInfo/backup boot
sector, FAT12 packing, dir entries, LFN, Minix/ext2, PDP-10 byte
pointers, littlefs, timestamps, EOC/dirty flags). An exFAT batch was
tangential and mostly confirmatory. A final confirmatory batch on
2026-09-18 (FAT32 format layout, FSInfo/backup boot sector,
FAT[0]/FAT[1] dirty flags, new fatgen103 mirror) returned >98%
already-covered material; the strict stop rule is formally satisfied
at 12 batches. (Correction: the footer as first written, and the
Pass 4 log entry, stated 8 batches; the true count was 11 at the
time of writing and 12 after the confirmatory batch — see the
research-log correction entry.)*
