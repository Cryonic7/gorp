# NE (New Executable) Format — GorpOS Reference

Purpose: the on-disk reference GorpOS needs to replace the Phase-2 NE loader
stub (currently returns failure — see `docs/INTERNALS.md`) with a real
Windows 3.1 NE loader. Covers header detection, every table, entry-bundle
encoding, relocation fixups, and the loader's responsibilities.

Scope: 16-bit NE as used by Windows 3.x (GorpOS's target). The same on-disk
format also serves OS/2 1.x, multitasking MS-DOS 4.x, and Windows 386; OS
differences are noted in §11.

> The authoritative source is Microsoft's own KB article Q65122
> ("INF: Executable-File Header Format", Windows 3.00 Developer's Notes),
> mirrored at `qb40/exe-format`. Facts from it and the osFree NE wiki are
> stated plainly; anything else is marked `[unverified]`.

---

## 1. What NE is

NE ("New Executable", also "segmented executable") is the 16-bit successor
to the DOS MZ format. Used by Windows 1.0–3.x (and 9x for 16-bit
components), multitasking ("European") MS-DOS 4.0, OS/2 1.x, the OS/2
subsystem of Windows NT up to Windows 2000, and as a container for `.fon`
bitmapped fonts. 32-bit Windows replaced it with PE; OS/2 2.0+ uses LX;
Windows 9x VxDs use LE. 64-bit Windows cannot run NE binaries at all.

Every NE file is a "fat binary": it **starts with a complete MZ header and
DOS stub program**, so it still runs (poorly) under plain DOS. The NE
header lives wherever the MZ header points.

---

## 2. Detecting NE

1. Bytes 0–1 = `MZ` (`0x5A4D`).
2. DWORD at MZ offset `0x3C` = `e_lfanew`: file offset of the real header.
   (Per the spec: the word at MZ offset `0x18` is the stub's relocation-table
   offset; when it is `0x40`, the DWORD at `0x3C` is the segmented-header
   offset.)
3. At that offset expect the signature word `NE`: low byte `'N'`, high byte
   `'E'` (`0x4E45`).

If the signature is absent, treat the file as an old-style MZ executable.
The DOS stub in between is irrelevant to the NE loader (it is never loaded
in Windows).

---

## 3. NE header (offsets relative to NE header start)

| Offset | Size | Name | Description |
|---|---|---|---|
| 00h | DW | ne_magic | Signature `"NE"` |
| 02h | DB | ne_ver | Linker version |
| 03h | DB | ne_rev | Linker revision |
| 04h | DW | ne_enttab | Entry Table offset, relative to NE header |
| 06h | DW | ne_cbenttab | Bytes in the entry table |
| 08h | DD | ne_crc | 32-bit CRC of file (computed with these words as 0); ignored in practice |
| 0Ch | DW | ne_flags | Flag word — see §4 |
| 0Eh | DW | ne_autodata | Automatic data segment number (1-based index into segment table); 0 if NOAUTODATA |
| 10h | DW | ne_heap | Initial dynamic-heap bytes added to the data segment (0 = none) |
| 12h | DW | ne_stack | Initial stack bytes added to the data segment (0 = none, or when SS≠DS) |
| 14h | DD | ne_csip | CS:IP as segment-number:offset (see §10 for DLLs) |
| 18h | DD | ne_sssp | SS:SP as segment-number:offset; if SS == autodata and SP == 0, SP is set to the top of the autodata segment just below the heap area |
| 1Ch | DW | ne_cseg | Number of segment-table entries |
| 1Eh | DW | ne_cmod | Number of module-reference-table entries |
| 20h | DW | ne_cbnrestab | Bytes in the non-resident name table |
| 22h | DW | ne_segtab | Segment Table offset, relative to NE header |
| 24h | DW | ne_rsrctab | Resource Table offset, relative to NE header |
| 26h | DW | ne_restab | Resident Name Table offset, relative to NE header |
| 28h | DW | ne_modtab | Module Reference Table offset, relative to NE header |
| 2Ah | DW | ne_imptab | Imported Names Table offset, relative to NE header |
| 2Ch | DD | ne_nrestab | Non-Resident Name Table offset, **relative to beginning of file** (not the NE header) |
| 30h | DW | ne_cmovent | Number of moveable entries in the Entry Table |
| 32h | DW | ne_align | Logical sector alignment shift: `log2` of segment sector size (default 9 = 512-byte sectors). Segment/resource file offsets are in these units |
| 34h | DW | ne_cres | Number of resource entries |
| 36h | DB | ne_exetyp | Executable type — see §5 |
| 37h | DB | ne_flagsothers | Operating-system flags `[unverified: bit definitions]` |
| 38h | DW | — | Return thunks, or start of gangload (fastload) area |
| 3Ah | DW | — | Segment-reference thunks, or length of gangload area (Windows) |
| 3Ch | DW | — | Minimum code swap area size (Windows) |
| 3Eh | 2 DB | — | Expected Windows version, **minor version first** (Windows) |

In-memory (Windows) the first bytes are reused: `02h` = usage count,
`06h` = selector to next module, `08h` = near ptr to DGROUP segment entry,
`0Ah` = near ptr to file info (`OFSTRUCT`). A GorpOS loader will build its
own in-memory module record; these fields document what Windows does.

---

## 4. `ne_flags` bits

| Bit(s) | Mask | Name | Meaning |
|---|---|---|---|
| 0–1 | — | NOAUTODATA | No automatic data segment (both bits clear) |
| 0 | 0001h | SINGLEDATA | Shared automatic data segment (per-process library data, shared DGROUP) |
| 1 | 0002h | MULTIPLEDATA | Instanced automatic data segment (per-instance library data) |
| 8 | 0100h | NENOTWINCOMPAT | Not compatible with PM windowing — fullscreen only (OS/2) |
| 9 | 0200h | NEWINCOMPAT | Compatible with PM windowing (OS/2) |
| 10 | 0300h | NEWINAPI | Uses PM windowing API (OS/2) |
| 11 | 0800h | FIRSTDISC / NEBOUND | First segment holds loader code (Windows) / Bound Family API (OS/2) |
| 13 | 2000h | LINKERROR | Link-time errors detected — module will not load |
| 14 | 4000h | NENOTMPSAFE | Valid stack not maintained (Windows) / not multiprocessor-safe (OS/2) |
| 15 | 8000h | LIBRARY | Module is a DLL — see §10 |

---

## 5. `ne_exetyp` values

| Value | Target |
|---|---|
| 00h | Unknown (any "new-format" OS) |
| 01h | OS/2 |
| 02h | **Windows** (GorpOS target) |
| 03h | European (multitasking) MS-DOS 4.x |
| 04h | Windows 386 |
| 05h | BOSS (Borland Operating System Services) |
| 81h | PharLap 286 DOS-Extender, OS/2 |
| 82h | PharLap 286 DOS-Extender, Windows |

The GorpOS loader should accept `02h` (and arguably `00h`); reject or
warn on others.

---

## 6. Segment table

`ne_cseg` entries of 8 bytes each. Entry 1 = segment number 1 (segment
numbers are 1-based everywhere in NE).

| Offset | Size | Name | Description |
|---|---|---|---|
| 00h | DW | ns_sector | Logical-sector offset (in `1 << ne_align` byte units) to the segment data, relative to file start. **0 = no file data** (pure BSS) |
| 02h | DW | ns_cbseg | Length of the segment in the file, in bytes. **0 = 64K** |
| 04h | DW | ns_flags | Flag word — see below |
| 06h | DW | ns_minalloc | Total allocation size of the segment in bytes. **0 = 64K** |

`ns_flags`:

| Mask | Name | Meaning |
|---|---|---|
| 0007h | TYPE_MASK | Segment-type field |
| 0000h | CODE | Code segment |
| 0001h | DATA | Data segment |
| 0010h | MOVEABLE | Segment is not fixed (can be moved/discarded and reloaded) |
| 0040h | PRELOAD | Segment is preloaded at startup; data segments with this are read-only |
| 0100h | RELOCINFO | Segment has relocation records (they follow the segment data in the file) |
| F000h | DISCARD | Discard priority (for the swapper) |

In-memory (Windows) entries grow an extra field at `08h`: `ns1_handle`,
the selector (or handle) of the segment in memory.

---

## 7. Resource table

Layout: `DW rs_align` (alignment shift for resource data offsets), then
type-information blocks, then per-type resource arrays, then strings.

**Type information block:**
- DW type ID: integer type if high bit set (`8000h`); otherwise an offset to
  the type string, relative to the resource-table start. **0 = end of type
  blocks.**
- DW number of resources of this type
- DD reserved

**Each resource (8 bytes):**
- DW file offset to the resource data, relative to file start, **in
  `rs_align` units**
- DW length of the resource in the file, in bytes
- DW flags: `0010h` MOVEABLE, `0020h` PURE (shareable), `0040h` PRELOAD
- DW resource ID: integer if `8000h` set, otherwise string offset relative to
  resource-table start
- DD reserved

**Strings** (type and name strings live at the end of the table): DB length
followed by that many ASCII bytes — **not NUL-terminated, case-sensitive**.
A zero length byte ends the string area and the resource table.

For a v1 GorpOS loader, resources can be deferred entirely (no Windows 3.1
program needs its icons to *run*); the table must still be parseable to
locate the tables that follow it.

---

## 8. Name tables, module references, imported names

All strings are Pascal-style: **DB length + bytes, not NUL-terminated,
case-sensitive**.

**Resident name table** (at `ne_restab`, relative to NE header): the
module's own name string first (its ordinal is ignored), then resident
exported procedure names. Each entry: DB length, ASCII bytes, **DW ordinal**
(index into the entry table). A zero length byte ends the table.

**Module reference table** (at `ne_modtab`): `ne_cmod` entries of 2 bytes.
Each is a DW offset **into the imported-names table** of a referenced
module's name string. Fixup records index this table to name the DLL an
import comes from.

**Imported names table** (at `ne_imptab`): concatenated length-prefixed
strings — module names and imported procedure names interleaved. Fixup
records point at offsets within this table.

**Non-resident name table** (at `ne_nrestab`, **relative to file start**):
same format as the resident table; the **first string is the module
description** (not the module name). Kept out of memory in Windows — the
loader reads it from disk on demand.

---

## 9. Entry table (bundles)

The entry table is the export address book: it maps **ordinals** to
segment:offset entry points. It is a sequence of *bundles*:

- **DB** number of entries in this bundle. **0 = end of the entry table.**
- **DB** segment indicator:
  - `00h` = unused bundle: no entry data; used by the linker to skip
    ordinal numbers. The next bundle follows immediately.
  - `01h`–`0FEh` = fixed segment number. Each entry is **3 bytes**:
    `DB` flags, `DW` offset within the segment. Flags: `01h` = exported,
    `02h` = uses global (shared) data segments (the entry prologue's first
    instruction must then be `MOV AX, data-segment-number`; only valid for
    SINGLEDATA libraries).
  - `0FFh` = moveable entries. Each entry is **6 bytes**: `DB` flags (same
    bits), **`INT 3Fh`** (`CD 3F` — a thunk stub the loader patches),
    `DB` segment number, `DW` offset within the segment.

Ordinal 1 = the first entry in the table. To resolve an ordinal, scan
bundles, subtracting each bundle's count; within the target bundle, index
by `(ordinal - 1) * entry_size`. Bundles are packed densely by the linker
(it may not reorder entries to improve bundling, because other modules
reference them by ordinal).

---

## 10. Per-segment data and relocation fixups

Segment data lives at file offset `ns_sector << ne_align`, `ns_cbseg`
bytes long, allocated as `ns_minalloc` bytes in memory (zero-fill the tail;
`ns_sector == 0` means allocate zeroed memory with no file data).

If `RELOCINFO` is set in `ns_flags`, relocation records **directly follow
the segment data in the file**: a `DW nr_nreloc` count, then that many
records:

| Offset | Size | Field |
|---|---|---|
| 00h | DB | nr_stype — source type (mask `0Fh`): `00h` LOBYTE, `02h` SEGMENT, `03h` FAR_ADDR (32-bit pointer), `05h` OFFSET (16-bit offset) |
| 01h | DB | nr_flags — target type (mask `03h`): `00h` INTERNALREF, `01h` IMPORTORDINAL, `02h` IMPORTNAME, `03h` OSFIXUP; plus `04h` ADDITIVE |
| 02h | DW | nr_soff — offset within the segment of the **source chain** |

**Source chain:** a linked list *inside the segment image* of every
location referencing the target, terminated by `0FFFFh`. The loader walks
it, patching each location. If ADDITIVE is set, the target value is
**added** to the existing contents instead of replacing them.

**Target formats** (bytes following `nr_soff`):

- **INTERNALREF** — reference inside this module:
  `DB` segment number (`0FFh` = moveable), `DB` 0,
  `DW` offset into the segment (fixed) or entry-table ordinal (moveable).
- **IMPORTORDINAL** — `DW` module-reference-table index, `DW` procedure
  ordinal in the exporting module.
- **IMPORTNAME** — `DW` module-reference-table index, `DW` offset into the
  imported-names table of the procedure-name string.
- **OSFIXUP** — `DW` fixup type, `DW` 0. Defined types are all floating-point
  emulator thunks: `0001h` FIARQQ/FJARQQ, `0002h` FISRQQ/FJSRQQ,
  `0003h` FICRQQ/FJCRQQ, `0004h` FIERQQ, `0005h` FIDRQQ, `0006h` FIWRQQ.
  (A GorpOS loader can defer these — they only matter for programs using
  the 8087-emulation thunks.)

**Iterated segment data:** some segments store run-length-encoded data as
`struct new_segdata`: a union of `{ WORD ns_niter; WORD ns_nbytes; BYTE
ns_iterdata[] }` (repeat `ns_nbytes` bytes `ns_niter` times) and raw
`{ BYTE ns_data[] }`. `[unverified: exact record-walking and
termination rule — confirm against a real iterated segment before
implementing.]`

---

## 11. What the loader must do (derived from the spec)

1. **Detect**: MZ → `e_lfanew` → `"NE"` signature (§2). Reject `ne_flags &
   2000h` (LINKERROR).
2. **Load segments**: for each of `ne_cseg` entries, allocate
   `ns_minalloc` bytes (0 → 64K), copy `ns_cbseg` bytes from
   `ns_sector << ne_align` (decode iterated data if present).
3. **Apply fixups**: for each RELOCINFO segment, read its fixup records and
   walk every source chain, resolving each target:
   - INTERNALREF → this module's own segment:offset (via entry table for
     moveable targets).
   - IMPORTORDINAL/IMPORTNAME → load the referenced module (module-reference
     table → imported-names table → module name), resolve the ordinal/name
     in *its* entry table (+ name tables), patch the chain with the
     imported segment:offset.
   - A program may only link against modules with the LIBRARY flag set —
     one program cannot dynamically link to another program.
4. **Instance data**: honor SINGLEDATA (shared DGROUP) vs MULTIPLEDATA
   (per-instance copy of the automatic data segment). `ne_autodata` names
   the DGROUP segment; DS points at the instance's copy.
5. **Heap/stack**: add `ne_heap` + `ne_stack` bytes above the loaded
   autodata image, laid out as the spec's diagram (top → bottom: additional
   dynamic heap, SP, additional stack, loaded autodata, DS/SS base). If
   SS == autodata and SP == 0, SP starts at the top of autodata just below
   the heap.
6. **Start execution**: for programs, far-jump to CS:IP
   (`ne_csip` = segment-number:offset). For DLLs (LIBRARY flag), SS:SP is
   invalid; instead **call** CS:IP as an initialization procedure with
   **AX = module handle** — it must far-return with AX ≠ 0 (success) or
   AX = 0 (failure). DS is the library's data segment if SINGLEDATA, else
   the caller's.
7. **Resources**: demand-loadable via the resource table; PRELOAD ones at
   startup; MOVEABLE/PURE/DISCARD guide the swapper. Deferrable in v1.

---

## 12. Windows vs OS/2 (and other NE users)

- The **on-disk format is identical** for all OSes; OS/2's use of NE is "just
  a subset" of the Windows format (per the author of the 2ine NE loader,
  who implemented both).
- `ne_exetyp` names the intended OS (§5). GorpOS targets `02h` (Windows).
- The `ne_flags` windowing bits (`0100h`/`0200h`/`0300h`) and `4000h` carry
  OS/2-specific meanings (Presentation Manager compatibility,
  multiprocessor-safety); the `0800h` bit is FIRSTDISC on Windows vs
  NEBOUND (Family API) on OS/2.
- The `38h`–`3Eh` header tail (gangload/fastload area, minimum code swap
  area, expected Windows version) is Windows-specific.
- 16-bit vs 32-bit: NE is fundamentally a 16-bit segmented format; OS/2 2.0+
  moved 32-bit programs to LX, Windows 95+ to PE. (NE "16/32-bit hybrid"
  binaries exist but are rare.)

---

## 13. GorpOS implementation notes

- **Phase 2 replaces the stub** (`docs/INTERNALS.md`): detection through
  `e_lfanew` already exists for MZ; add the `"NE"` signature check.
- **Segment model**: NE segments are 16-bit (≤64K). On GorpOS's 32-bit
  protected-mode kernel this means one 16-bit code/data descriptor per
  segment (LDT or GDT) with base = segment's linear address — the same
  approach Wine and hobby loaders use. MOVEABLE segments can initially be
  treated as fixed.
- **Suggested staging**: (1) header parse + segment load + fixup
  application + entry-point jump for simple NOAUTODATA/SINGLEDATA programs
  with no imports; (2) import resolution across modules + DLL init protocol;
  (3) MULTIPLEDATA instancing; (4) resources, discardable-segment swapping,
  gangload, OSFIXUP float thunks — all deferrable.
- **What NOT to build in v1**: resource loading, the swapper/DISCARD
  priorities, gangload, iterated-segment decoding (verify rule first),
  OSFIXUP emulation.
- **Testing**: `.fon` files are NE containers with no code — good parse
  tests. Small Windows 3.1 SDK sample programs are good load tests.

---

## 14. Known gaps / follow-up

- `ne_flagsothers` (37h) bit definitions. `[unverified]`
- Exact iterated-segment-data record walk/termination rule. `[unverified]`
- Module-reference-table index base (0- vs 1-based) in IMPORTORDINAL/
  IMPORTNAME targets. `[unverified — likely 1-based, confirm against a
  real binary]`
- In-memory module-table layout details beyond the header-prefix reuse
  (usage count, next-module selector, DGROUP pointer). Deferrable — GorpOS
  defines its own.
- Exact semantics of the `02h` "global data segments" entry flag beyond the
  `MOV AX, seg` prologue rule. `[unverified]`

---

## 15. Sources

Exact URLs returned by research searches (2026-09-18):

- Microsoft KB Q65122, "INF: Executable-File Header Format" (Windows 3.00
  Developer's Notes) — **authoritative spec**, mirrored:
  `https://github.com/qb40/exe-format`
- osFree wiki, "New Executable file format" (header offsets, `ne_exetyp` /
  `ne_flags` / `ne_flagsothers` tables, segment entries, iterated data,
  fixup records):
  `https://ftp.osfree.org/doku/doku.php?id=en:docs:tk:formats:newexe&do=export_pdf`
- Wikipedia, "New Executable" (history, OS coverage, PE/LX succession):
  `http://en.wikipedia.org/wiki/New_Executable`
- Wikipedia, "Comparison of executable file formats" (NE as x86-only,
  MZ-stub fat binary, no 64-bit):
  `http://en.wikipedia.org/wiki/Comparison_of_executable_file_formats`
- 2ine project notes (OS/2's NE is a subset of the Windows format;
  segments vs LX memory blocks):
  `https://www.patreon.com/posts/2ine-16-bit-exe-19337541`
- Dr. Dobb's, "Examining OS/2 2.1 Executable File Formats" (NE fixup tables
  appended to their segments; Pascal-style name strings):
  `https://jacobfilipp.com/DrDobbs/articles/DDJ/1994/9409/9409h/9409h.htm`
- MS-DOS EXE format notes (`e_lfanew` at MZ offset 60):
  `http://fileformats.archiveteam.org/index.php?title=MS-DOS_EXE&`

Pre-existing project pointers (not re-verified this pass):
`https://github.com/dosworld/toc/blob/HEAD/DOCS/NE.MD`,
`https://github.com/phaelonimaire/claude-os2-toolkit/blob/HEAD/os2ref/executable-formats.md`,
`https://github.com/devbrain/mz-explode`
