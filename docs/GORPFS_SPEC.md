# GorpFS v0 — Normative On-Disk Specification

**Status:** normative specification, v0. Version `0x0001`.
**Authority:** this document is the single source of truth for the
GorpFS v0 on-disk format. Where it conflicts with the design sketch
in `docs/FILESYSTEM.md` Part 3, this document wins (all known
conflicts were resolved during writing; none remain).
**Scope:** format only. Implementation (`mkfs.gorp`, `fsck.gorp`,
kernel driver) is staged work; nothing here is code.

**Conventions:** RFC 2119 language — MUST / MUST NOT / REQUIRED /
SHALL / SHALL NOT / SHOULD / SHOULD NOT / RECOMMENDED / MAY /
OPTIONAL — is used deliberately throughout. "v0 decision" marks a
choice made here where the design sketch was silent; the rationale
is given inline.

**Global invariants (from the design sketch, restated normatively):**

- G1. GorpFS is a **byte-oriented on-disk format by construction.**
  All on-disk metadata is fixed-width little-endian integers and
  byte arrays. There are no packed cells anywhere in the
  superblock, inodes, or directory entries.
- G2. File data is an **octet stream, always**. `size` is always in
  octets. The cell model (8/10/13-bit cells) exists only in memory;
  the disk never knows about it.
- G3. Canonical bit order for every packed bit-stream on disk
  (bitmaps, cell-native blobs): **LSB-first within each byte,
  bytes in little-endian order** — item *i* of width *w* occupies
  stream bits `[i·w, i·w+w)`, stream bit 0 = bit 0 of byte 0.
- G4. 512-byte sectors. Block = 8 sectors = 4096 bytes (v0; the
  `sectors_per_block` parameter exists but only 8 is valid in v0).
- G5. Foreign binaries (DOS/Win32/Linux) MUST always see a strict
  8-bit view (§6). The cell model MUST NOT leak into any
  foreign-visible interface.

---

## 1. Volume layout

```
Block 0   Sector 0:    primary superblock (512 B)
          Sectors 1–7: boot/code area (reserved)
Block 1                boot/code area (reserved, 4 KiB)
Block 2                backup superblock (first 512 B; rest zero)
Block B_bmap           block bitmap
Block B_imap           inode bitmap
Block B_itab ..        inode table (128-byte inodes, 32 per block)
Block B_data ..        data blocks
[Block N-1]            optional third superblock (features bit 1)
```

Block numbers are u32 values; byte offset of block *b* =
*b · sectors_per_block · 512*. Block 0 is never allocatable as
data: as a block pointer value, **block 0 = hole** (sparse), and the
allocator MUST never return it. Inode numbers are **1-based**;
inode 1 is the root directory; inode 0 means "none".

The superblock fields `block_bitmap`, `inode_bitmap`,
`inode_table`, and `data_start` give the block numbers of each
region. Nothing except sector 0 is at a hardcoded offset.

Minimum volume: `block_count` ≥ 16, `inode_count` ≥ 16. `mkfs.gorp`
MUST refuse smaller geometries; readers MUST validate
`data_start < block_count` and that every region pointer lies
within `[0, block_count)`.

### 1.1 Disambiguation from FAT

Sector 0 carries the magic `"GORPFS01"` at offset 0 and `0x55 0xAA`
at offset `0x1FE` (firmware politeness only). A FAT probe MUST check
for the GorpFS magic **first**; only if the magic is absent AND
`0x55AA` is present MAY it probe for FAT. `mkfs.gorp` MUST refuse
to format a volume whose sector 0 has a plausible FAT signature
(`0x55AA` at `0x1FE` plus a jump opcode `0xEB`/`0xE9` at offset 0)
unless `--force` is given.

---

## 2. Superblock (sector 0, 512 bytes)

All multi-byte integers are **little-endian**. All offsets are
bytes from the start of sector 0.

| Offset | Size | Name | Semantics |
|--------|------|------|-----------|
| 0x00 | 8 | `magic` | MUST be the 8 ASCII bytes `"GORPFS01"`. Readers MUST check this before trusting any other field. |
| 0x08 | 2 | `version` | On-disk format version. v0 = `0x0001`: high byte = major (0), low byte = minor (1). See §8 for acceptance rules. |
| 0x0A | 2 | `sectors_per_block` | Sectors per block. MUST be 8 in v0 (4 KiB blocks). Readers MUST reject any other value in v0. |
| 0x0C | 4 | `block_count` | Total blocks in the volume, including all metadata. MUST be ≥ 16. |
| 0x10 | 4 | `inode_count` | Total inodes in the inode table. MUST be ≥ 16. Table occupies `ceil(inode_count·128 / 4096)` blocks starting at `inode_table`. |
| 0x14 | 4 | `block_bitmap` | Block number of the block bitmap. |
| 0x18 | 4 | `inode_bitmap` | Block number of the inode bitmap. |
| 0x1C | 4 | `inode_table` | First block number of the inode table. |
| 0x20 | 4 | `data_start` | First block number usable for file data. MUST be > `inode_table` region end. Blocks `[data_start, block_count)` are the allocation pool. Blocks `[0, data_start)` are metadata and MUST be marked allocated in the bitmap. |
| 0x24 | 4 | `root_inode` | Inode number of the root directory. MUST be 1 in v0. |
| 0x28 | 4 | `free_blocks` | Last known count of free blocks. `0xFFFFFFFF` = unknown (recompute by scanning). A hint only — see §5. |
| 0x2C | 4 | `free_inodes` | Last known count of free inodes. `0xFFFFFFFF` = unknown. |
| 0x30 | 4 | `next_free_block` | Allocation hint: block number where the next first-fit scan starts. `0xFFFFFFFF` = no hint, scan from `data_start`. Advisory only — see §5. |
| 0x34 | 2 | `state` | `1` = clean (last unmount orderly), `2` = dirty (mounted, or not cleanly unmounted). All other values are invalid; readers MUST treat them as dirty. |
| 0x36 | 2 | `features` | Feature flags. Bit 0 (`0x0001`) = `HAS_BACKUP_SB`: backup superblock at block 2 is present (always set by `mkfs.gorp`). Bit 1 (`0x0002`) = `HAS_THIRD_SB`: third copy at block `block_count-1` is present. Bits 2–15 are reserved and MUST be zero in v0. See §8 for unknown-bit policy. |
| 0x38 | 4 | `mkfs_version` | Version of the `mkfs.gorp` that created the volume: high 16 bits = major, low 16 = minor. Informational; readers MUST NOT gate behavior on it. |
| 0x3C | 16 | `uuid` | Volume UUID, RFC 4122 version 4, generated at mkfs time. MUST be nonzero. Used to tell volumes apart (fstab-by-UUID, fsck targeting). |
| 0x4C | 16 | `label` | Volume label, UTF-8, NUL-padded. Up to 16 bytes; MAY be unterminated if all 16 bytes are non-NUL. Readers MUST NOT read past 16 bytes. Informational only. |
| 0x5C | 4 | `checksum` | CRC-32 (ISO 3309 / zlib variant) over bytes `0x00`–`0x5B`, stored little-endian. See §2.1. |
| 0x60 | 4 | `sb_seq` | Superblock generation counter, incremented on every superblock rewrite. Lets recovery pick the newest valid copy (§3). *v0 decision:* the sketch left the reserved area unstructured; a sequence number is the minimal mechanism that makes multi-copy recovery well-defined. |
| 0x64 | 346 | `reserved` | MUST be zero when written; MUST be ignored when read. |
| 0x1FE | 2 | — | MUST be `0x55 0xAA` (bytes `0x55`, `0xAA` in that order). |

### 2.1 Checksum algorithm (normative)

*v0 decision — the sketch said "CRC32 (or simple sum in v0)". We
choose CRC-32, not a bare sum: a sum catches single-bit errors
poorly and costs the same table-driven plain-C code; ext4's
`metadata_csum` precedent confirms CRC-family checksums as the
industry norm for on-disk metadata.*

- Algorithm: **CRC-32/ISO-HDLC** (the zlib/PKZIP/Ethernet CRC):
  polynomial `0x04C11DB7` (reflected form `0xEDB88320`),
  initial value `0xFFFFFFFF`, input reflected, output reflected,
  final XOR `0xFFFFFFFF`.
- Coverage: bytes `0x00` through `0x5B` inclusive (92 bytes) of
  the 512-byte sector. (The checksum field itself at `0x5C` is
  outside the covered range, so no zeroing games are needed.)
- Stored as u32 little-endian at `0x5C`.
- Reference check vector: CRC-32 of the 92 zero bytes is
  `0x8F95843C` *[unverified — computed from the standard
  definition, not cross-checked against an implementation in this
  pass]*. `mkfs`/`fsck` host round-trip tests MUST assert the
  standard test vector CRC-32(`"123456789"`) = `0xCBF43926`
  (established) before trusting their own implementation.
- Verification order on mount/probe: magic → version/features →
  checksum → geometry sanity. A checksum failure means "do not
  trust this copy"; it is not by itself proof of a GorpFS volume
  (see §3 recovery).

### 2.2 State flag discipline

- `mkfs.gorp` writes `state = 1` (clean).
- On read-write mount, the driver MUST set `state = 2` (dirty)
  and flush the superblock copies **before** performing any
  allocation or data write.
- On orderly unmount, after all data, bitmaps, and inode writes
  are flushed, the driver MUST set `state = 1` and flush the
  superblock copies.
- Mounting a volume with `state = 2` (unclean last unmount) is
  permitted for read-write (there is no journal to replay in v0),
  but the driver SHOULD log a warning and the operator SHOULD run
  `fsck.gorp` at the next opportunity. Read-only mount of a dirty
  volume is always permitted and MUST NOT alter `state`.
- Ordered-write discipline for metadata updates (v0,
  crash-honest): data blocks → block/inode bitmaps → inode table
  → backup superblock(s) → **primary superblock last**. The
  primary is the commit point; a torn write can at worst leave a
  stale-but-checksummed primary behind the backups, which §3
  resolves via `sb_seq`.

---

## 3. Backup superblock(s)

- **Backup #1** is REQUIRED when `features & HAS_BACKUP_SB`
  (always, in practice): a byte-identical 512-byte copy of the
  primary superblock at **block 2, offset 0**. Bytes 512–4095 of
  block 2 are reserved and MUST be zero.
- **Backup #2** is present iff `features & HAS_THIRD_SB`: a
  byte-identical 512-byte copy at **block `block_count - 1`,
  offset 0**; the rest of that block MUST be zero. `mkfs.gorp`
  sets this bit (and writes the copy) when `block_count ≥ 65536`
  (≥ 256 MiB); MAY set it on smaller volumes; MUST NOT set it
  when `block_count - 1` would collide with the inode table or
  bitmaps (impossible at ≥ 16 blocks, but stated for safety).
- **What must match:** every present copy MUST be byte-identical
  to the primary, including `checksum` and `sb_seq`. The kernel
  updates all copies on every superblock write (backup(s) first,
  primary last per §2.2).
- **Recovery procedure** (`fsck.gorp`, and the kernel's
  mount-time fallback):
  1. Read sector 0. If magic matches AND checksum verifies, it
     is a candidate; record its `sb_seq`.
  2. Read block 2 (+ last block if `HAS_THIRD_SB` was readable
     from any valid copy; if no copy is valid yet, probe the
     last block too — its location needs only the device size).
     Validate each the same way.
  3. If no copy validates: the volume is unrecoverable by this
     procedure — report and stop.
  4. If exactly one copy validates: use it; `fsck` rewrites the
     missing/invalid copies from it.
  5. If several validate but differ: use the one with the
     highest `sb_seq` (newest); `fsck` resynchronizes the others
     to it and logs a warning. `sb_seq` ties (should not happen)
     resolve in favor of the primary.
  6. After any resynchronization, `fsck` re-verifies all copies.

---

## 4. Inode table

Inode *n* (1-based) lives at byte offset `(n-1)·128` from the
start of block `inode_table`. Inode 0 does not exist. Each inode
is 128 bytes:

| Offset | Size | Name | Semantics |
|--------|------|------|-----------|
| 0x00 | 2 | `mode` | Unix-style type + permission bits. High 4 bits: `0x4000` directory, `0x8000` regular, `0xA000` symlink, `0x1000` FIFO, `0xC000` socket, `0x2000` char device, `0x6000` block device. Low 12 bits: `0x800` setuid, `0x400` setgid, `0x200` sticky, `0x100`–`0x001` = `rwxrwxrwx`. For regular/dir/symlink inodes the type bits MUST agree with `ftype`. |
| 0x02 | 2 | `nlink` | Hard link count. Regular files: ≥ 1 while linked. Directories: 2 + number of subdirectories (`.` and `..` convention). `fsck` verifies against a counted reference. |
| 0x04 | 2 | `uid` | Owner user id. `0` = root. v0 has no id-mapping; stored raw. |
| 0x06 | 2 | `gid` | Owner group id. `0` = root. |
| 0x08 | 2 | `flags` | Reserved for v1 (`IMMUTABLE`, `APPEND` candidates). MUST be zero in v0. |
| 0x0A | 8 | `size` | File size in **octets** (u64). For directories: multiple of 4096 (§5.4). For symlinks: length of the target path in octets. |
| 0x12 | 8 | `atime` | Last access time, Unix epoch seconds (u64). |
| 0x1A | 8 | `mtime` | Last data-modification time, Unix epoch seconds. |
| 0x22 | 8 | `ctime` | Last status-change time (inode metadata change), Unix epoch seconds. |
| 0x2A | 1 | `width_hint` | Preferred in-memory cell width for this file's data: `8` (default), `10`, or `13`. `0` means 8. See §4.1. |
| 0x2B | 1 | `ftype` | `0` unknown, `1` regular, `2` directory, `3` symlink, `4` FIFO, `5` socket, `6` char device, `7` block device. `8–255` reserved. See §4.2. |
| 0x2C | 4 | `reserved` | MUST be zero. |
| 0x30 | 40 | `direct[10]` | Ten direct data-block numbers (u32 each). `0` = hole (sparse — reads as zeros, occupies no block). |
| 0x58 | 4 | `indirect` | Single-indirect block number: block of 1024 u32 block numbers. `0` = none. |
| 0x5C | 4 | `dindirect` | Double-indirect block number: block of 1024 u32 pointers to single-indirect blocks. `0` = none. (No triple-indirect in v0.) |
| 0x60 | 32 | `reserved2` | MUST be zero. Extension room: v1 xattr block pointer goes at `0x60`. |

Maximum file size in v0: `(10 + 1024 + 1024²) · 4096` =
`4,299,202,560` bytes (~4 GiB). Indirect blocks contain u32 LE
block numbers; unused slots MUST be zero. `fsck` MUST verify that
no two inodes claim the same block and that every claimed block
is marked allocated in the bitmap.

### 4.1 `width_hint` (normative)

- Valid values: `0`, `8`, `10`, `13`. `0` MUST be treated as `8`.
- It is a **hint to the VFS about in-memory materialization
  width** (§6.4). It changes nothing on disk: the bytes are
  identical regardless of the hint.
- A reader encountering any other value MUST treat it as `8`
  (fail-safe to the octet contract — never guess a cell geometry)
  and SHOULD warn. A writer MUST NOT store any other value.
- For non-regular files (directory, symlink, FIFO, socket,
  device) the hint is meaningless: writers MUST store `8`
  (`0` also acceptable, normalized to 8 by readers); readers
  MUST ignore it.
- Rationale (*v0 decision*): widths are a closed set because the
  cell layer only implements 8/10/13; failing safe to 8 keeps an
  unknown-hint file readable as octets by every consumer,
  including foreign binaries.

### 4.2 `ftype` values (normative)

*v0 decision — the sketch enumerated 0–3 and asked whether
symlink/device/fifo/socket belong in v0. Answer: the format
reserves the full ext2-style range 0–7 so the encoding never has
to change, but v0 only activates regular, directory, and symlink.
The rest are parseable-but-inert.*

| Value | Name | v0 status |
|-------|------|-----------|
| 0 | `FT_UNKNOWN` | Permitted; `fsck` infers from `mode` type bits and MAY fill it in. |
| 1 | `FT_REG` | Regular file. Fully supported. |
| 2 | `FT_DIR` | Directory. Fully supported. |
| 3 | `FT_SYMLINK` | Symbolic link. Fully supported; target stored as octets (§4.3). |
| 4 | `FT_FIFO` | Named pipe. Format-valid; VFS returns `ENOTSUP` on open in v0 (staged). |
| 5 | `FT_SOCK` | Socket. Format-valid; `ENOTSUP` on open in v0 (staged). |
| 6 | `FT_CHRDEV` | Character device. Format-valid; `ENOTSUP` on open in v0 (staged). |
| 7 | `FT_BLKDEV` | Block device. Format-valid; `ENOTSUP` on open in v0 (staged). |
| 8–255 | reserved | Reader MUST treat as `FT_UNKNOWN`; `fsck` MUST flag. |

For device inodes (6/7): `direct[0]` = device major (u32),
`direct[1]` = device minor (u32); all other pointer fields MUST
be zero; `size` MUST be zero. (*v0 decision:* two dedicated u32
slots is simpler than ext2's packed encoding and leaves the
pointer array untouched.)

### 4.3 Symlinks

- A symlink's target path is stored as **exactly `size` octets**
  of file data (ordinary data blocks), NOT NUL-terminated.
- The target is interpreted as bytes: absolute if it begins with
  `/` (`0x2F`), otherwise relative to the containing directory.
- `width_hint` MUST be 8 for symlinks (§4.1); the target is never
  cell-packed.
- Maximum target length: bounded by the max file size; `fsck`
  MUST reject symlink loops at lookup time per the usual
  `MAXSYMLINKS` (40) depth limit — a VFS rule, stated here so
  `fsck`'s offline loop check matches it.

---

## 5. Directories

### 5.1 Entry layout

Variable-length entries, ext2-style, packed densely from the
start of each 4 KiB directory block:

```
+------------+----------+----------+----------+------------------+
| ino (u32)  | reclen   | namelen  | ftype    | name bytes       |
| 4 B  LE    | (u16 LE) | (u8)     | (u8)     | namelen B        |
+------------+----------+----------+----------+------------------+
  offset +0    offset +4  offset +6  offset +7  offset +8
```

- `ino`: inode number (1-based). `0` = free slot.
- `reclen`: total length of this entry **including padding**,
  in bytes. MUST be a multiple of 4, MUST be ≥ 8, and
  `offset + reclen` MUST NOT exceed the block end.
- `namelen`: length of the name in bytes, 1–255. The entry's
  minimum size is `round4(8 + namelen)`; any extra bytes in
  `reclen` are padding and MUST be zero.
- `ftype`: copy of the target inode's `ftype` (§4.2), so
  `readdir` need not fetch the inode. MAY be `FT_UNKNOWN` (0);
  consumers MUST then stat the inode.
- `name`: raw bytes, NOT NUL-terminated.

### 5.2 Name constraints (normative)

- Length 1–255 bytes.
- MUST NOT contain `0x00` (NUL) or `0x2F` (`/`).
- The names `"."` and `".."` are reserved for the dot entries;
  userspace MUST NOT create them (`mkdir`/`create` return
  `EEXIST`/`EINVAL` — implementation detail, stated so `fsck`
  knows they can only come from directory creation).
- Names are opaque bytes to the filesystem: no case folding, no
  normalization, no Unicode equivalence. v0 tools SHOULD accept
  any UTF-8; v0 tools MAY additionally restrict to printable
  ASCII, but the on-disk format does not.
- *v0 decision:* forbidding `/` and NUL is the minimal sane
  rule (same as every Unix); deeper character policy is a
  userspace concern, not a format concern.

### 5.3 Dot entries

- Every directory **except the root** begins with exactly two
  entries: `"."` (`ino` = own inode number) then `".."`
  (`ino` = parent inode number).
- The root directory (inode 1) has NO dot entries (*per the
  design sketch*, mirroring the FAT12/16 root convention).
- `".."` in a child of the root points at inode 1.

### 5.4 Directory sizing and iteration

- A directory's `size` MUST be a whole multiple of 4096; growth
  is by whole blocks (*v0 decision:* simplifies the iteration
  invariant — a directory block is always complete).
- An empty directory block is a single free entry:
  `ino = 0, reclen = 4096`. A fresh non-root directory block
  holds `"."` (`reclen = 12`), `".."` (`reclen = 12`), then one
  free entry with `reclen = 4096 - 24` absorbing the rest. The
  root's fresh block is a single free entry, `reclen = 4096`.
- **Iteration algorithm** (normative for readers): for each data
  block of the directory in order, set `off = 0`; while
  `off < 4096`: read `reclen` at `off + 4`; validate
  (`reclen ≥ 8`, `reclen % 4 == 0`, `off + reclen ≤ 4096`);
  if `ino != 0`, yield the entry; `off += reclen`. A zero or
  insane `reclen` MUST abort iteration with an error, not skip
  blindly (this is where ext2's `ext2_check_page` earned its
  keep).
- The entries in a block MUST exactly tile it:
  `Σ reclen == 4096`. `fsck` verifies this.

### 5.5 Insertion and deletion (normative)

- **Insertion:** scan for a free entry (`ino == 0`) with
  `reclen ≥ round4(8 + namelen)`; split it (new entry takes the
  needed size, remainder becomes a free entry with the leftover
  `reclen`). If none, take the LAST entry in the last block,
  shrink its `reclen` to its minimal `round4(8 + namelen)`, and
  place the new entry in the freed tail space; if that fails,
  append a new directory block. (This is the ext2 `add_link`
  algorithm, restated.)
- **Deletion:** set the entry's `ino = 0`. Then coalesce: if the
  PREVIOUS entry in the same block is free (`ino == 0`), add this
  entry's `reclen` to the previous entry's `reclen` (the deleted
  entry vanishes). Otherwise the entry stays as a free slot.
  Coalescing MUST NOT cross block boundaries. If the deleted
  entry is the first in the block, it simply becomes
  `ino = 0` with its `reclen` unchanged.
- Name bytes of deleted entries SHOULD be zeroed (hygiene;
  not required for correctness).
- Directories are never implicitly compacted across blocks in
  v0; a trailing all-free block MAY be truncated by `fsck`
  (reducing `size`), but the kernel is not required to.

---

## 6. Block and inode allocation

### 6.1 Bitmap format

- **Block bitmap:** bit *n* ↔ block *n*. Byte `n div 8`, bit
  `n mod 8` (LSB-first, §G3): bit 0 of byte 0 = block 0.
  `1` = allocated, `0` = free. Size = `ceil(block_count / 8)`
  bytes; bits beyond `block_count` MUST be set (allocated) —
  they are not real blocks and MUST never be handed out.
- **Inode bitmap:** bit `n - 1` ↔ inode *n* (1-based). `1` =
  allocated. Size = `ceil(inode_count / 8)` bytes; excess bits
  MUST be set.
- At `mkfs` time, all metadata blocks `[0, data_start)` and
  inode 1 are marked allocated. Block 0's bit is set (it is the
  superblock sector's block) AND block 0 is never returned by
  the allocator (double protection: the hole convention and the
  bitmap agree).

### 6.2 Allocation policy (normative)

- First-fit starting from `next_free_block`, wrapping around
  the pool `[data_start, block_count)`:
  `b = data_start + ((start - data_start + i) mod (block_count - data_start))`
  for `i = 0, 1, …`. The first free bit wins.
- `start = next_free_block` if it lies in
  `[data_start, block_count)`; otherwise `start = data_start`.
- The allocator MUST verify the bitmap bit before returning a
  block; the hint is advisory. If the hint points at an
  allocated block, the allocator MUST rescan from `data_start`
  and repair the hint (set it to the block actually returned).
- On alloc: set bit, `free_blocks--` (saturating; if
  `free_blocks == 0xFFFFFFFF`, leave it unknown), set
  `next_free_block = b + 1` (wrapping is handled by the scan).
- On free: clear bit, `free_blocks++` (unless unknown), and if
  `b < next_free_block`, set `next_free_block = b` (freed
  blocks become the new hint — the FAT32 `FSI_Nxt_Free` idea).
- Exhaustion: if the scan completes with no free block, return
  `ENOSPC`. `next_free_block` is then set to `0xFFFFFFFF`.
- **Hint/bitmap disagreement:** the bitmap is authoritative,
  always. `fsck` recomputes `next_free_block` as the lowest free
  block ≥ `data_start` (or `0xFFFFFFFF` if none) and recomputes
  `free_blocks`/`free_inodes` by full scan, fixing the
  superblock.

### 6.3 Inode allocation

- First-fit over the inode bitmap starting from the lowest
  free bit (no hint field in v0 — inode allocation is rare
  enough that a linear scan is fine; *v0 decision*).
- On alloc: set bit, `free_inodes--`, zero the 128-byte inode
  image before filling fields.
- On free (link count reaches 0 and no open handles): clear
  bit, `free_inodes++`, zero the inode image.

---

## 7. The 8-bit foreign-view translation layer

This section is the normative contract between GorpFS and every
foreign binary (DOS/Win32/Linux programs, and any 8-bit-only
consumer). It implements WORK_PROPOSAL decision D6 and
COMPILERS.md §4 consequence 2: *the cell model MUST NOT leak
into compat ABIs.*

### 7.1 The contract, in full

1. **File data is octets, unconditionally.** A foreign `read()`
   on any regular file returns exactly the on-disk octet stream:
   bytes `[0, size)` as stored. `width_hint` is invisible —
   there is no foreign-visible API (no ioctl, no stat field, no
   directory metadata) that exposes it. A 10-bit-packed file
   reads as its packed octets, period.
2. **Directory listings are octet names.** `readdir` returns
   `(d_ino, d_name, d_type)` where `d_name` is the raw name
   bytes and `d_type` derives from `ftype`. No width metadata
   is attached to entries.
3. **Metadata mapping** (`stat`/`fstat`, POSIX view):
   | Foreign field | GorpFS source |
   |---|---|
   | `st_ino` | inode number |
   | `st_mode` | `mode` verbatim (type + permission bits) |
   | `st_nlink` | `nlink` |
   | `st_uid` / `st_gid` | `uid` / `gid` (identity mapping in v0) |
   | `st_size` | `size` (octets) |
   | `st_blksize` | 4096 |
   | `st_blocks` | allocated data blocks × 8 (512-byte units; holes not counted) |
   | `st_atime/mtime/ctime` | epoch u64 → `tv_sec`; `tv_nsec = 0` (v0 has no sub-second stamps) |
   | `st_dev` | volume identity derived from `uuid` (implementation picks the hash; the value MUST be stable per mount) |
4. **Symlinks are octet paths.** `readlink` returns exactly
   `size` octets. The foreign program resolves them as byte
   strings; no width conversion ever applies.
5. **Foreign writes are octet writes.** A foreign `write()`
   appends/inserts octets; the file's `width_hint` is left
   unchanged. Consequence (documented, not prevented): if a
   foreign program writes raw bytes into a `width_hint = 10`
   file, a later cell-native reader will unpack those octets
   as 10-bit cells — garbage in, garbage out, exactly as with
   any binary format mismatch. The filesystem does not police
   this; the marshalling contract is the application's.
6. **The marshall points** are exactly two functions, and all
   cell↔octet conversion happens inside them:
   - `cells_to_octets(cells, w)` — packs cell array to octets
     per §G3; used on the native write path.
   - `octets_to_cells(octets, w)` — unpacks per §G3; used on
     the native read path.
   - Round-trip property (MUST hold, tested by `mkfs`/`fsck`
     host tests): `octets_to_cells(cells_to_octets(x)) == x`
     for every width `w ∈ {8, 10, 13}`.
   - The foreign path calls neither function. The native path
     calls exactly one per crossing. There is no third path.

### 7.2 Worked example: 10-bit packed pixel buffer

A cell-native tool writes 4 pixels as 10-bit cells with
`width_hint = 10`:

```
cells = [0x001, 0x200, 0x3FF, 0x123]   (w = 10)
```

**Packing** (`cells_to_octets`, §G3: cell *i* → stream bits
`[10i, 10i+10)`, stream bit 0 = bit 0 of byte 0):

```
cell0 = 0x001 = 00 00000001
cell1 = 0x200 = 10 00000000
cell2 = 0x3FF = 11 11111111
cell3 = 0x123 = 01 00100011      (bits 9..0)

stream bits  0–7:  cell0[7:0]              = 00000001 → byte0 = 0x01
stream bits  8–15: cell0[9:8]=00, cell1[5:0]=000000 → byte1 = 0x00
stream bits 16–23: cell1[9:6]=1000, cell2[3:0]=1111 → byte2 = 0xF8
stream bits 24–31: cell2[9:4]=111111, cell3[1:0]=11  → byte3 = 0xFF
stream bits 32–39: cell3[9:2]=01001000                → byte4 = 0x48
```

On-disk file data (5 octets), `size = 5`, inode `width_hint = 10`:

```
01 00 F8 FF 48
```

**(a) Cell-layer read** (native program): VFS sees
`width_hint = 10`, allocates a 10-bit region, calls
`octets_to_cells(bytes, 10)` → `[0x001, 0x200, 0x3FF, 0x123]`.
Round-trip holds.

**(b) DOS program read** (foreign): opens the file, reads 5
octets `01 00 F8 FF 48`. It knows nothing of cells, widths, or
hints — to it, this is just a 5-byte binary file. `stat`
reports `st_size = 5`. The 8-bit view is total.

The same 5 octets serve both masters because the packing is
canonical (§G3) and the width is metadata, not encoding.

---

## 8. mkfs.gorp / fsck.gorp contract

### 8.1 What `mkfs.gorp` MUST write (field by field)

1. Sector 0: `magic = "GORPFS01"`; `version = 0x0001`;
   `sectors_per_block = 8`; geometry as computed;
   `block_bitmap`/`inode_bitmap`/`inode_table`/`data_start`
   block numbers; `root_inode = 1`; `free_blocks`/`free_inodes`
   = exact counts; `next_free_block = data_start`;
   `state = 1`; `features = HAS_BACKUP_SB (| HAS_THIRD_SB
   if block_count ≥ 65536)`; `mkfs_version` = tool version;
   `uuid` = fresh v4; `label` as given (NUL-padded);
   `checksum` = computed CRC-32; `sb_seq = 1`;
   `reserved` = zero; `0x55AA` at `0x1FE`.
2. Sectors 1–7 of block 0 and all of block 1: zeroed (boot/code
   area reserved for the stage-2 payload; `mkfs` MUST NOT place
   filesystem structures there).
3. Block 2: 512-byte superblock copy, rest zero. Last block:
   same, iff `HAS_THIRD_SB`.
4. Bitmaps: metadata blocks `[0, data_start)` set, pool clear;
   excess bits set; inode bit 0 (inode 1) set.
5. Inode table: all zero; then inode 1 = root directory:
   `mode = 0x41ED` (dir + 0755), `nlink = 2`, `uid = gid = 0`,
   `size = 4096`, timestamps = now, `width_hint = 8`,
   `ftype = 2`, `direct[0]` = first data block, rest zero.
6. Root's data block: single free entry (`ino = 0`,
   `reclen = 4096`) — the root has no dot entries (§5.3).
7. `mkfs` MUST verify its own output by re-reading the
   superblock and checking the checksum before exiting
   success (self-check rule).

### 8.2 What `fsck.gorp` MUST verify (checklist)

Offline only in v0 (no online scrubbing). Order:

1. **Superblock:** magic; `version` acceptable (§9);
   `features` known (§9); checksum verifies; geometry sane
   (`sectors_per_block == 8`, `16 ≤ block_count`,
   `16 ≤ inode_count`, region pointers in range and
   non-overlapping, `data_start` sane); `0x55AA` present;
   `root_inode == 1`.
2. **Backup copies:** validate per §3; resynchronize from the
   highest-`sb_seq` valid copy; warn on any divergence.
3. **Bitmaps:** size correct; excess bits set; every block
   claimed by an inode (direct/indirect/dindirect, including
   indirect blocks themselves) is marked; no block is claimed
   twice; every marked data block is claimed by exactly one
   inode (else: offer to clear, or reconnect to `lost+found`
   — *v0 decision:* `fsck` reconnects orphaned inodes to
   `/lost+found`, creating it if needed, ext2-style).
4. **Inodes:** `ftype` in 0–7; `mode` type bits agree with
   `ftype` for 1/2/3; `width_hint` in {0,8,10,13} (fix others
   to 8 with warning); `size` ≤ max file size; directory
   `size` is a multiple of 4096; `nlink` equals counted links
   (fix); timestamps are sane (not absurdly far future —
   warn only); device inodes have sane pointer fields;
   `reserved`/`flags`/`reserved2` zero (warn, do not fail).
5. **Directories:** entries tile each block (`Σ reclen ==
   4096`); `reclen` sane; `namelen` in 1–255; names contain no
   NUL/`/`; `ino` references a valid allocated inode;
   `ftype` matches target (fix); dot entries correct
   (`.` = self, `..` = parent; absent in root); no duplicate
   names within a directory.
6. **Counts and hints:** recompute `free_blocks`/`free_inodes`
   by scan; fix superblock; recompute `next_free_block`.
7. **State:** if `state == 2`, run the full check, then set
   `state = 1` and resync all superblock copies on clean pass.
8. Exit status: `0` = clean, `1` = errors corrected,
   `2` = errors remain / unrecoverable (documented so scripts
   can gate mounts on it).

---

## 9. Versioning and forward compatibility

- `version`: high byte = major, low byte = minor. v0 =
  `0x0001` (major 0, minor 1).
- A reader MUST accept a volume iff `major` equals its own
  major AND `minor ≤` its own minor. It MUST refuse otherwise
  (major bump = incompatible layout change; higher minor =
  newer than this reader).
- `features` (u16): bit 0 `HAS_BACKUP_SB`, bit 1
  `HAS_THIRD_SB` are defined in v0. **Unknown feature bits
  are incompat-class in v0:** if any bit outside the
  reader's known set is set, the driver MUST refuse to mount
  (read-write AND read-only) and `fsck` MUST report
  "unsupported feature bits" and refuse repair. *v0 decision
  with ext2 precedent:* ext2/e2fsprogs refuse to touch
  volumes with unknown INCOMPAT bits ("has unsupported
  feature(s)… Get a newer version of e2fsck"), and the kernel
  refuses the mount ("couldn't mount … due to feature
  incompatibilities"). v0 adopts the strict form — refuse
  everything — because silent partial support is how
  filesystems get corrupted. A future v1 MAY split this into
  compat / read-only-compat / incompat classes; the 16-bit
  field has room, and such a split would be a minor-version
  bump, which current readers would then refuse per the rule
  above. That is the intended upgrade path, not a loophole.
- **Reserved-field policy:** writers MUST zero every
  `reserved` field; readers MUST ignore them; `fsck` reports
  nonzero reserved bytes as warnings, never as errors
  (forward-compat: v1 may define them, and a v0 `fsck` MUST
  NOT "repair" fields it doesn't understand).
- `mkfs_version`, `uuid`, `label`: informational; MUST NOT
  affect mount decisions.

---

## Appendix A — Normative C structure sketches

Informative (the tables above are normative; these must match
them). Assumes `CHAR_BIT == 8`, little-endian host — true for
every target in CPU_SUPPORT.md.

```c
#define GORPFS_MAGIC "GORPFS01"
#define GORPFS_VERSION 0x0001
#define GORPFS_SPB 8

struct gorpfs_sb {
    char     magic[8];        /* 0x00 "GORPFS01" */
    uint16_t version;         /* 0x08 */
    uint16_t sectors_per_block; /* 0x0A = 8 */
    uint32_t block_count;     /* 0x0C */
    uint32_t inode_count;     /* 0x10 */
    uint32_t block_bitmap;    /* 0x14 */
    uint32_t inode_bitmap;    /* 0x18 */
    uint32_t inode_table;     /* 0x1C */
    uint32_t data_start;      /* 0x20 */
    uint32_t root_inode;      /* 0x24 = 1 */
    uint32_t free_blocks;     /* 0x28 */
    uint32_t free_inodes;     /* 0x2C */
    uint32_t next_free_block; /* 0x30 */
    uint16_t state;           /* 0x34 1=clean 2=dirty */
    uint16_t features;        /* 0x36 bit0 backup SB, bit1 third SB */
    uint32_t mkfs_version;    /* 0x38 */
    uint8_t  uuid[16];        /* 0x3C */
    char     label[16];        /* 0x4C */
    uint32_t checksum;         /* 0x5C CRC-32 over 0x00..0x5B */
    uint32_t sb_seq;          /* 0x60 generation counter */
    uint8_t  reserved[346];   /* 0x64 */
    uint8_t  sig[2];          /* 0x1FE 0x55 0xAA */
};
_Static_assert(sizeof(struct gorpfs_sb) == 512, "sb size");

struct gorpfs_inode {
    uint16_t mode;            /* 0x00 */
    uint16_t nlink;           /* 0x02 */
    uint16_t uid;             /* 0x04 */
    uint16_t gid;             /* 0x06 */
    uint16_t flags;           /* 0x08 = 0 in v0 */
    uint64_t size;            /* 0x0A octets */
    uint64_t atime;           /* 0x12 */
    uint64_t mtime;           /* 0x1A */
    uint64_t ctime;           /* 0x22 */
    uint8_t  width_hint;      /* 0x2A 0/8/10/13 */
    uint8_t  ftype;           /* 0x2B 0..7 */
    uint32_t reserved;        /* 0x2C = 0 */
    uint32_t direct[10];      /* 0x30 */
    uint32_t indirect;        /* 0x58 */
    uint32_t dindirect;       /* 0x5C */
    uint8_t  reserved2[32];   /* 0x60 = 0 */
};
_Static_assert(sizeof(struct gorpfs_inode) == 128, "inode size");

struct gorpfs_dirent {
    uint32_t ino;             /* +0 */
    uint16_t reclen;          /* +4 */
    uint8_t  namelen;         /* +6 */
    uint8_t  ftype;           /* +7 */
    char     name[];           /* +8 */
};
```

## Appendix B — v0 decisions index

Choices made in this document where the design sketch was
silent (all marked inline as "v0 decision"):

1. Checksum = CRC-32/ISO-HDLC over `0x00`–`0x5B` (§2.1).
2. `sb_seq` generation counter at `0x60` for backup-copy
   arbitration (§2, §3).
3. `features` bit 1 = `HAS_THIRD_SB` at last block for
   `block_count ≥ 65536` (§3).
4. Unknown `width_hint` fails safe to 8 (§4.1).
5. `ftype` 4–7 format-valid but `ENOTSUP` in v0; device
   major/minor in `direct[0]`/`direct[1]` (§4.2).
6. Symlink target = exactly `size` octets, no NUL; 40-level
   loop limit (§4.3).
7. Directory size always a multiple of 4096; empty block =
   single free entry (§5.4).
8. Deleted-entry name zeroing is SHOULD, not MUST (§5.5).
9. Inode allocation = plain first-fit, no hint field (§6.3).
10. Unknown feature bits refuse ALL mounts in v0 (strict
    incompat-class); v1 may adopt ext2-style tri-class (§9).
11. Nonzero reserved fields = `fsck` warning, never error (§9).
12. Orphaned inodes reconnected to `/lost+found` (§8.2).
13. `fsck` exit codes 0/1/2 (§8.2).
14. Minimum geometry: 16 blocks, 16 inodes (§1).

## Sources

- GorpFS design sketch + marshalling contract: `docs/FILESYSTEM.md`
  Part 2 (§2.3) and Part 3 (this spec's parent document).
- Foreign 8-bit view requirement: `docs/WORK_PROPOSAL.md` D6;
  `docs/COMPILERS.md` §4.
- ext2 directory/inode/bitmap design (precedent for entries,
  deletion coalescing, `lost+found`, feature-bit refusal):
  [ext2 design (Tytso)](http://web.mit.edu/tytso/www/linux/ext2intro.html);
  FAT spec for dirty-flag and backup-sector precedent:
  [fatspec.pdf (UVa mirror)](https://www.cs.virginia.edu/~cr4bd/4414/F2019/files/fatspec.pdf).
- Unknown-feature refusal behavior confirmed via e2fsprogs/kernel
  reports ("has unsupported feature(s)… Get a newer version of
  e2fsck",
  [experts-exchange](https://www.experts-exchange.com/questions/28296568/Linux-ext4-error-hard-drive-partition-is-missing-and-hdd-is-not-mounting.html);
  "couldn't mount as ext3 due to feature incompatibilities",
  [snbforums](https://www.snbforums.com/threads/usb-drive-not-mounted-and-read-only-error.85266/)).
- Ordered-write / metadata-pair atomicity (staged, not v0):
  [littlefs DESIGN.md](https://github.com/littlefs-project/littlefs/blob/master/DESIGN.md).

*Research: 2 confirmatory search batches for this avenue (ext2
dir-entry deletion semantics; unknown-feature-bit mount policy),
both >98% non-novel against Avenue 5's 12 batches plus
established knowledge. The CRC-32 check vector for 92 zero bytes
is marked [unverified] pending host-test confirmation.*
