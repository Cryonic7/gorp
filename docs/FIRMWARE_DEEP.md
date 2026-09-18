# GorpOS Firmware Deep Dive — Multiboot 1/2 and the coreboot payload model

Companion to FULL_SCOPE.md (Layer 1) and RESEARCH.md §4, §9. Docs-only
research: everything below is either web-verified (URLs cited verbatim as
returned by search) or flagged [unverified].

## 1. Multiboot 1 (Specification 0.6.96)

### 1.1 Header

The header must be **completely within the first 8192 bytes** of the OS
image, **longword (32-bit) aligned**, and should come "as early as
possible" — typically embedded at the start of the text segment after the
executable's own header. All fields little-endian
([spec](https://www.gnu.org/software/grub/manual/multiboot/multiboot.html)).

| Offset | Field | Size | Notes |
|---|---|---|---|
| 0 | `magic` | u32 | Must be `0x1BADB002` |
| 4 | `flags` | u32 | Feature request bitfield (see below) |
| 8 | `checksum` | u32 | Must satisfy `magic + flags + checksum ≡ 0 (mod 2³²)`, i.e. `checksum = -(magic + flags)` |
| 12 | `header_addr` | u32 | Physical address of the header itself (only if `flags[16]`) |
| 16 | `load_addr` | u32 | Physical address the image body starts loading at (if `flags[16]`) |
| 20 | `load_end_addr` | u32 | Physical address load ends; **0 means "load whole file"** (if `flags[16]`) |
| 24 | `bss_end_addr` | u32 | Physical address where BSS ends; loader zeroes BSS (if `flags[16]`) |
| 28 | `entry_addr` | u32 | Physical entry-point address (if `flags[16]`) |
| 32 | `mode_type` | u32 | 0 = linear graphics, 1 = EGA-standard text (if `flags[2]`) |
| 36 | `width` | u32 | Preferred width in chars/pixels (if `flags[2]`) |
| 40 | `height` | u32 | Preferred height (if `flags[2]`) |
| 44 | `depth` | u32 | Preferred bits-per-pixel, 0 for text (if `flags[2]`) |

**Flags.** Bits 0–15 are *requirements* (boot loader must fail loudly if
it cannot honor them); bits 16–31 are *optional*. All undefined bits must
be zero.

| Bit | Meaning |
|---|---|
| 0 | Page-align (4 KB) all boot modules |
| 1 | Provide memory info — at least `mem_*`; `mmap_*` must also be provided if the loader can build one |
| 2 | Provide video mode table info + set the preferred mode in `mode_type/width/height/depth` |
| 16 | Header address fields (offsets 12–28) are present; loader uses them instead of ELF program headers |

The classic minimal flags value is `0x00000003` (page-align modules +
memory info) for ELF images; for non-ELF images bit 16 is typically set
too (`0x00010003`), as in GRUB's own `multiboot.h`
([source](https://android.googlesource.com/platform/external/grub/+/refs/heads/ics-plus-aosp/docs/multiboot.h)).

### 1.2 Machine state at handoff

On x86 the loader calls the kernel entry point in **32-bit protected
mode, paging disabled, interrupts disabled**; the spec explicitly
requires the PIC be left programmed with the normal BIOS/DOS values and
that boot data structures (GDT/IDT used by the loader) not be clobbered
until the OS is done reading them. Register contract:

- `EAX` = `0x2BADB002` — "Multiboot-compliant boot loader" magic
- `EBX` = physical address of the **Multiboot information structure**
- The OS must create its own stack immediately

### 1.3 Boot information structure (at `EBX`)

All offsets below are from the struct start; presence of each field is
gated by the corresponding bit in `flags` (offsets relative to struct base).

| Offset | Field | Valid if | Meaning |
|---|---|---|---|
| 0 | `flags` | always | Presence/validity bitmask |
| 4 | `mem_lower` | flags[0] | Lower memory in KB (0 → 640 KB max) |
| 8 | `mem_upper` | flags[0] | Upper memory in KB, starting at 1 MB (up to first memory hole) |
| 12 | `boot_device` | flags[1] | BIOS drive: byte0 = INT 13h drive (0x80 = first HDD); bytes 1–3 = DOS/BSD partition chain, 0xFF = unused |
| 16 | `cmdline` | flags[2] | Physical address of zero-terminated kernel command line |
| 20 | `mods_count` | flags[3] | Number of boot modules (may be 0) |
| 24 | `mods_addr` | flags[3] | Physical address of module-struct array |
| 28–40 | `syms` | flags[4] or flags[5] | a.out (`tabsize/strsize/addr/reserved`) or ELF (`num/size/addr/shndx`) symbol info |
| 44 | `mmap_length` | flags[6] | Total size of the memory-map buffer |
| 48 | `mmap_addr` | flags[6] | Address of the memory-map buffer |
| 52–56 | `drives_*` | flags[7] | BIOS drive structures (CHS/LBA geometry) |
| 60 | `config_table` | flags[8] | ROM configuration table |
| 64 | `boot_loader_name` | flags[9] | Loader name string pointer |
| 68 | `apm_table` | flags[10] | APM BIOS table |
| 72–86 | `vbe_*` | flags[11] | VBE control info, mode info, mode, protected-mode interface |
| 88 | `framebuffer_addr` | flags[12] | u64 physical address of framebuffer |
| 96 | `framebuffer_pitch` | flags[12] | Bytes per scanline |
| 100 | `framebuffer_width` | flags[12] | Pixels/columns |
| 104 | `framebuffer_height` | flags[12] | Lines |
| 108 | `framebuffer_bpp` | flags[12] | Bits per pixel |
| 109 | `framebuffer_type` | flags[12] | 0 = indexed color, 1 = direct RGB, 2 = EGA text |
| 110–115 | `color_info` | flags[12] | Palette or RGB mask fields depending on type |

**Memory map entries** (each: `size` prefix, then the entry; `size` may
exceed 20 for forward extension):

| Offset | Field | Size | Meaning |
|---|---|---|---|
| 0 | `base_addr` | u64 | Region start (physical) |
| 8 | `length` | u64 | Region size in bytes |
| 16 | `type` | u32 | 1 = available RAM; 2 = reserved; 3 = ACPI reclaimable; 4 = NVS (preserve on hibernate); 5 = defective RAM; else reserved |

**Module struct** (each 16 bytes at `mods_addr`): `mod_start` (u32),
`mod_end` (u32), `string` (u32, zero-terminated ASCII — typically a
command line or path; may be 0), `reserved` (u32, 0).

## 2. Multiboot 2 (Specification 2.0)

### 2.1 Header

The header must be **8-byte aligned** and located **within the first 32768
(32 KiB) bytes** of the image. Structure:

| Offset | Field | Size | Notes |
|---|---|---|---|
| 0 | `magic` | u32 | `0xE85250D6` |
| 4 | `architecture` | u32 | 0 = i386 (32-bit protected mode entry); also defines MIPS |
| 8 | `header_length` | u32 | Total header length incl. all tags |
| 12 | `checksum` | u32 | `magic + architecture + header_length + checksum ≡ 0 (mod 2³²)` (four fixed header words only — *not* the tags) |

Then a list of **tags**: each `u16 type | u16 flags | u32 size`,
**8-byte aligned**, payload padded; terminated by an **end tag**
(type 0, size 8). Defined header tags (from the spec TOC):
1 = information request (kernel lists which *info tags* it wants, e.g.
mmap/framebuffer), 2 = address tag (like MB1 address fields),
3 = entry address tag, 4 = console flags, 5 = framebuffer (preferred
width/height/depth), 6 = module alignment (modules must be page-aligned),
7 = EFI boot services, 8 = EFI i386 entry address, 9 = EFI amd64 entry
address, 12 = relocatable header.

### 2.2 Machine state at handoff (i386)

- `EAX` = `0x36D76289` (Multiboot2-compliant loader magic)
- `EBX` = **32-bit physical address** of the Multiboot2 information structure
- `CS` = 32-bit read/execute flat segment (base 0, limit 0xFFFFFFFF);
  `DS/ES/FS/GS/SS` = 32-bit read/write flat segments
- A20 gate enabled; `CR0`: PG (bit 31) cleared, PE (bit 0) set;
  `EFLAGS`: VM (bit 17) and IF (bit 9) cleared
- **The OS must create its own stack immediately** — nothing about ESP
  is guaranteed
- Boot loader must not load any part of kernel/modules/info above
  `4 GiB − 1` (most tags only support 32-bit addresses)
  ([spec text](https://www.gnu.org/software/grub/manual/multiboot2/multiboot.txt))

### 2.3 Boot information structure (at `EBX`)

Header: `total_size` (u32), `reserved` (u32); first tag begins at
**offset 8**; tags walked by `ptr += align8(tag->size)` until an end tag
(type 0, size 8). Info tag types: 0 end, 1 boot command line, 2 boot
loader name, **3 modules** (base/end addresses + string), 4 basic meminfo
(lower/upper KB), 5 BIOS boot device, **6 memory map** (entries with 64-bit
base/length, types 1 available / 3 ACPI reclaimable / 4 NVS / 5 badram,
else reserved), 7 VBE info, **8 framebuffer info** (addr/pitch/width/height/
bpp/type + palette/RGB fields), 9 ELF sections, 10 APM, 11/12 EFI system
tables, 13 SMBIOS, 14/15 ACPI RSDP, 16 networking, 17 EFI memory map,
18 EFI boot services not terminated, 19/20 EFI image handles,
21 image load base physical address.
([spec TOC](https://www.gnu.org/software/grub/manual/multiboot2/multiboot.txt),
practical tag-walking example
[here](https://github.com/baponkar/osdev-notes/blob/HEAD/01_Build_Process/02_Boot_Protocols.md))

## 3. Making the GorpOS kernel Multiboot-compliant

Our current boot chain (per WORK_PROPOSAL.md §work plan): 512-byte MBR
boot sector → BIOS INT 13h disk reads → E820 map → GDT → CR0.PE →
protected mode → bootstrap paging → higher-half `kmain`. Making GRUB
boot us instead changes the *first* part of this chain; the kernel body
(paging onward) is untouched.

### 3.1 Minimal MB1 addition (week-scale, v1-real)

1. **Add a `.multiboot` section in boot asm** (before any code), with
   `align 4`, the three required dwords (`0x1BADB002`, flags
   `0x00000003`, checksum), and the five address fields with
   `flags |= 0x10000` (bit 16). The five address fields are required
   whenever the kernel is a flat/non-ELF image (as the current custom
   512-byte boot-sector + stage-2 chain implies), because the loader has
   no ELF program headers to consult: `header_addr` = where the header
   lands, `load_addr`/`load_end_addr`/`bss_end_addr` describe the
   physical footprint, `entry_addr` = our 32-bit entry stub. If the
   kernel is ever shipped as a valid ELF image instead, bit 16 is
   unnecessary — GRUB derives the layout from the program headers.
2. **Entry stub contract** (MB1): assume protected mode, paging off,
   interrupts off, PIC at BIOS values; set up a stack immediately;
   save `EAX`/`EBX` (magic + info pointer); validate `EAX ==
   0x2BADB002`; read `mem_*`/`mmap_*`/cmdline/modules from `EBX`
   **before** the kernel's allocator claims any RAM (the loader may
   place the info struct anywhere — copy the mmap entries and cmdline
   into kernel-owned memory first, then enable paging).
3. **Linker layout**: pin `.multiboot` early enough that it falls in
   the first 8192 bytes (put the section first after any raw-binary
   header). With GRUB, the GDT/protected-mode transition and A20 are
   the *loader's* job; our stage-1 protected-mode entry code becomes
   unreachable in the GRUB path — keep it behind a "BIOS path" entry
   symbol.
4. **Testing**: `grub-mkrescue` ISO + `qemu-system-i386`; QEMU's
   `-kernel` option also loads Multiboot images directly (handy
   smoke test without building an ISO).

### 3.2 Minimal MB2 addition (same effort class)

1. Add a `.multiboot2` section: magic `0xE85250D6`, arch 0,
   length, checksum, plus three tags — information-request (ask for
   memory map, ELF sections, framebuffer), entry-address, and end tag.
   8-byte align the whole section; pin it within the first 32 KiB.
2. Entry stub reads `EAX`/`EBX`, walks the tag list, extracts the
   framebuffer tag (feeds our fb layer — this replaces the VGA-BIOS
   dependency for mode setup) and the memory map (feeds the
   width-tagged allocator's region table).
3. **Both headers can coexist**: the MB1 and MB2 headers are independent
   scans (first 8 KiB vs first 32 KiB); GRUB honors the MB2 one when
   `multiboot2` is used in grub.cfg. This is how kernels support both.

### 3.3 What stays BIOS

The MBR stage remains the *fallback and the teaching path*: full
BIOS-boot path (sector load → real-mode → protected-mode) stays intact
behind its own entry point, selected by a build flag or a boot-menu
choice in stage 1. Multiboot only *replaces* the loader half; it cannot
exist without a BIOS/firmware underneath anyway — GRUB itself is BIOS-
or UEFI-booted. Keeping both means: QEMU-bios path for learning/debugging
triple faults with full control, GRUB/MB path for real hardware, ISO
images, and multi-boot.

## 4. coreboot payload model

### 4.1 Architecture

coreboot does **hardware init only** (chipset, DRAM training, Super I/O,
device enumeration) and then jumps to a **payload** — "coreboot doesn't
try to mandate how the boot process should look, it merely does hardware
init and then passes on control to another piece of software that we
carry along in firmware storage" — stored as a file in **CBFS** (coreboot
filesystem) inside the ROM image
([payloads doc](https://github.com/coreboot/coreboot/blob/HEAD/Documentation/payloads.md)).

### 4.2 Payload formats and the SELF loader

- **ELF**: "ELF compatible static linked binaries can be loaded as
  payload. ELF binaries are loaded through the SELF boot mechanism."
  The SELF loader reads the ELF headers from CBFS, copies segments to
  their physical addresses, and jumps to `e_entry`. Any statically
  linked 32-bit ELF kernel with a sane entry stub qualifies — this is
  also how a custom OS becomes a payload with no special format work.
- **Payload API** (for payload-to-payload chaining, from libpayload's
  `i386_do_exec`): pushes `argc`/`argv`, a magic `0x00BC614E`
  (`12345678` decimal) sanity marker on the stack, then jumps to the
  target address — a documented convention payloads use to hand off to
  each other ([coreboot mailing list archive](https://mail.coreboot.org/hyperkitty/list/coreboot@coreboot.org/thread/K4BO67TED5QIFHBJVYNY7DWQYKE6SNG2/)).
- **libpayload** is the support library payloads build against: small
  libc (string/memory/stdio), console drivers (serial, VGA text),
  PCI access, CBFS access, LZMA decompression, optional curses. It
  parses **coreboot tables** into `struct sysinfo_t` — notably the
  memory map — so payloads don't need their own E820 walk. It can
  *also* parse Multiboot tables into the same structure
  ([r3673 commit](https://mail.coreboot.org/hyperkitty/list/coreboot@coreboot.org/thread/557BA4XLB77TQBWJ3LLWC2XWC5WQTQ4L/?sort=date)).

### 4.3 SeaBIOS as a payload

[SeaBIOS](https://www.seabios.org) (open PCBIOS implementation) can be
built as a coreboot payload: it "supports executing Option ROMs in a
more complete fashion than coreboot. It also supports Multiboot."
Chain: coreboot → SeaBIOS payload → legacy boot (disk MBR, option ROMs)
or a Multiboot image. This is how real hardware regains BIOS INT
services (INT 10h/13h) that coreboot alone deliberately omits, and it's
the same SeaBIOS that QEMU uses as its default BIOS
([payloads doc](https://github.com/coreboot/coreboot/blob/HEAD/Documentation/payloads.md),
[SeaBIOS build overview](https://github.com/coreboot/seabios/blob/master/docs/Build_overview.md)).

### 4.4 GRUB2 as a payload

GRUB2 "was originally written as a bootloader … but it can also be
compiled as a coreboot payload," giving a coreboot+GRUB stack that can
then Multiboot-boot the kernel — an alternative to SeaBIOS for systems
where legacy BIOS services aren't needed.

### 4.5 What a custom OS payload needs

1. A **statically linked 32-bit ELF** with a valid `e_entry`; build
   with `CONFIG_PAYLOAD_ELF` ("An ELF executable payload") and the
   file is added to CBFS via `cbfstool add-payload`
   ([coreboot tutorial](https://github.com/coreboot/coreboot/blob/HEAD/Documentation/tutorial/part1.md)).
2. **No BIOS services**: at payload entry the CPU is in 32-bit
   protected mode; there is no IVT, no INT 10h/13h. Early console =
   serial (COM1) or direct VGA-text writes. Our framebuffer console
   plan covers this; Mode 13h-via-BIOS does not.
3. **Memory map**: read the **coreboot tables** (linked list in low
   memory, pointer passed to the payload) — or link libpayload and use
   its sysinfo parsing. Do *not* assume E820 is available.
4. **No Multiboot info structure is provided** — a raw ELF payload
   does not get `EBX`→info like GRUB gives. If our kernel wants its
   MB2 tag parsing to work under coreboot, either (a) chain through
   GRUB-as-payload or SeaBIOS+GRUB (then the kernel sees a normal MB2
   handoff), or (b) add a coreboot-tables reader beside the MB2 tag
   reader in the early boot code. Both headers being present costs
   nothing; the parser that finds valid data wins.

## 5. Recommendation

**Multiboot-compliant kernel (MB1 + MB2 headers in one image) as the
staged primary boot contract; keep our BIOS MBR path as the permanent
fallback; coreboot-native stays epic.**

Rationale:

1. **Multiboot is the highest-leverage boot contract.** One header
   addition (~40 lines of asm + linker pinning) unlocks GRUB, QEMU
   `-kernel`, SeaBIOS-as-payload chains, and ISO/USB installs. It is
   the documented Phase 3 "staged" milestone in FULL_SCOPE.md §Layer 1,
   and research found nothing to contradict that ordering.
2. **Keep BIOS boot as fallback, not as the only path.** Our MBR →
   real-mode → protected-mode chain is the learning artifact and the
   no-dependencies rescue path. Multiboot and BIOS are not in conflict:
   the kernel body is identical; only the first-stage entry differs.
   A build-time switch (`BOOT=multiboot|bios`) plus a runtime magic
   check (`EAX == 0x2BADB002 / 0x36D76289` vs our own boot signature)
   selects the parser.
3. **coreboot-native is not worth pursuing before real hardware
   exists.** Per-board months (chipset init, DRAM training, Super I/O)
   for educational payoff only — already labeled epic in FULL_SCOPE.md.
   When that day comes, the Multiboot-compliant image drops in as a
   CBFS ELF payload with ~zero changes, ideally chained through
   SeaBIOS or GRUB-as-payload so the kernel sees its familiar MB2
   handoff. No coreboot-specific code is needed in the kernel for v1
   of that path; a coreboot-tables memory-map reader is the only
   foreseen addition, and only if the SeaBIOS/GRUB chain is skipped.
4. **Prefer MB2 as the primary contract, MB1 as compat.** GRUB2's
   `multiboot2` is the actively maintained path, its tag model is
   extensible (framebuffer info feeds our graphics staging directly),
   and QEMU `-kernel` handles both. MB1 costs one more small header
   and buys compatibility with older loaders — cheap insurance.

*Methodology:* batch-pass searches; citations are URLs returned verbatim
by the search tool. Multiboot 1 facts from the 0.6.96 spec text; MB2
facts from the 2.0 spec text and two hobby-OS implementation writeups
that reproduce the spec's constants; coreboot facts from coreboot's own
`Documentation/payloads.md`, the coreboot tutorial, and the libpayload
commit history. Nothing here required an invented source.
