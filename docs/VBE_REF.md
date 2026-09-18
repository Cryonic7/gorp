# VBE/VGA Programming Reference (GorpOS graphics stage)

Scope: everything a hobby OS needs to bring up graphics on P4-era
hardware without a GPU driver — VGA Mode 13h, VGA text mode, and the
VESA BIOS Extensions (VBE 2.0/3.0) real-mode interface, plus how to
reach it from protected mode. Sources are the VBE 3.0 standard itself
and OSDev/community references; all facts below are sourced from the
docs and the web results cited, not from memory. Anything I could not
verify is marked `[unverified]`.

Related: `docs/GRAPHICS.md` (stage plan), `docs/GRAPHICS_COMPAT.md`
(syscalls), `docs/RESEARCH.md` §8 (graphics overview).

---

## 1. What VBE is

VBE = **VESA BIOS Extensions**: a standardized set of INT 10h services
(`AH=4Fh`) layered on the VGA BIOS, so programs can enumerate and set
super-VGA modes without per-card knowledge. For our graphics stage,
VBE is the path from 320x200x256 (Mode 13h, settable through plain
BIOS `AH=00h`) to high-resolution **linear framebuffer** modes, all
without touching real GPU hardware.

Canonical spec: [VESA VBE Core Functions 3.0 (PDF, Sep 1998)](https://git.guld-berg.dk/SingOS/SingOS/raw/branch/lets32bit/References/VESA_BIOS_Extensions_%28VBE%29_v3.pdf)
(VBE 2.0 text mirrored at [phatcode.net](http://www.phatcode.net/res/221/files/vbe20.pdf)).
Overview: [Wikipedia — VESA BIOS Extensions](https://en.wikipedia.org/wiki/VESA_BIOS_Extensions).
Classic tutorial: [SuperVGA/VESA programmer's notes (faqs.org)](http://www.faqs.org/faqs/pc-hardware-faq/supervga-programming/).

## 2. VBE return status (AX)

All INT 10h `AH=4Fh` functions return status in AX (32-bit PM
variants don't return status — see §9):

| AL / AH | Meaning |
|---|---|
| AL == 4Fh | Function supported |
| AL != 4Fh | Function not supported |
| AH == 00h | Call successful |
| AH == 01h | Call failed |
| AH == 02h | Not supported in current hardware configuration |
| AH == 03h | Call invalid in current video mode |

Treat *any* nonzero AH as failure (later VBE revisions may define
more codes). Source: VBE 3.0 spec, "VBE Return Status".

## 3. INT 10h `AX=4Fxx` function reference

Notation: I = input registers, O = output. "preserved" = all other
registers preserved per the spec.

### 4F00h — Return VBE Controller Information (required)

- I: AX=4F00h; ES:DI → 512-byte `VbeInfoBlock` (caller-provided buffer).
  Preset the `VbeSignature` field to ASCII `'VBE2'` to request VBE 2.0+
  extended info; the block is 256 bytes for VBE 1.x, 512 for 2.0+.
- O: AX = status. On success `VbeSignature` becomes `'VESA'`.
- Layout (offsets): 0 `VbeSignature` db 4 ('VESA'); 4 `VbeVersion`
  dw (BCD: 0x0300 = 3.0, 0x0102 = 1.2); 6 `OemStringPtr` dd (vbeFarPtr);
  10 `Capabilities` db 4; 14 `VideoModePtr` dd (vbeFarPtr to
  FFFFh-terminated list of mode numbers); 18 `TotalMemory` dw
  (64 KB units: 4 = 256 KB); 20+ VBE 2.0+ fields
  (`OemSoftwareRev`, vendor/product name+rev pointers); 256
  `OemData` db 256.
- `Capabilities` bits (D0–D4 defined): D0 = DAC width switchable
  6→8 bits; D1 = controller not VGA-compatible when set;
  D2 = program RAMDAC during blank via Function 09h BL=80h (old
  RAMDACs, avoids "snow"); D3 = stereo signaling supported;
  D4 = EVC-connector stereo; D5–31 reserved.

### 4F01h — Return VBE Mode Information (required)

- I: AX=4F01h; CX = mode number; ES:DI → 256-byte `ModeInfoBlock`
  (caller buffer, pre-zeroed).
- O: AX = status; block filled. If the mode is unavailable,
  ModeAttributes D0 is cleared.
- Always returns exactly 256 bytes. Full field layout in §5.

### 4F02h — Set VBE Mode (required)

- I: AX=4F02h; BX = mode number; if BX bit D11 set, ES:DI →
  `CRTCInfoBlock` (refresh-rate programming, optional).
- BX bits: D0–D8 = mode number; D9–D10 reserved (0);
  D11 = 1 → use CRTC values in ES:DI (else BIOS default);
  D12–D13 reserved; **D14 = 1 → linear/flat framebuffer** (fails if
  unavailable); **D15 = 1 → preserve display memory** (0 = clear).
- O: AX = status. On failure the BIOS must leave the environment
  unchanged.
- Standard VGA 7-bit modes can be passed in BL with BH cleared.

### 4F03h — Return Current VBE Mode (required)

- I: AX=4F03h. O: AX = status; BX = current mode (D0–D13 mode,
  D14 = linear model, D15 = memory not cleared at last set).
- Not supported via the VBE 3.0 protected-mode entry point.

### 4F04h — Save/Restore State (required)

- I: AX=4F04h; DL = 00h query size / 01h save / 02h restore;
  CX = requested-state mask (D0 controller hw, D1 BIOS data,
  D2 DAC, D3 registers; 000Fh = everything except framebuffer);
  ES:BX → buffer (if DL != 0).
- O: AX = status; BX = number of 64-byte blocks needed (DL=00h).
- Superset of the old INT 10h AH=1Ch state save. For our use:
  save/restore the palette + CRTC when switching between text and
  graphics modes.

### 4F05h — Display Window Control / bank switching (required)

- I: AX=4F05h; BH = 00h set / 01h get window;
  BL = window number (00h Window A, 01h Window B);
  DX = window number in *granularity units* (set only).
- O: AX = status; DX = current window (get only).
- 32-bit variant: same BH/BL/DX, plus ES = selector for
  memory-mapped registers (only if the 4F0Ah ports/memory subtable
  lists any). Bank switches fail with AH=03h while in a linear
  framebuffer mode.
- The `ModeInfoBlock.WinFuncPtr` gives a real-mode far pointer to
  the bank-switch routine callable directly (same BH/BL/DX args) to
  skip INT 10h overhead. Sketch in §7.

### 4F06h — Set/Get Logical Scan Line Length (required)

- I: AX=4F06h; BL = 00h set in pixels / 01h get / 02h set in bytes /
  03h get max; CX = desired width (pixels or bytes).
- O: AX = status; BX = bytes per scan line; CX = actual pixels
  (truncated); DX = max scan lines available. Too-wide CX → AH=02h.
- Lets you build a logical buffer wider than the display (panning
  with 4F07h). Valid in VBE text modes too.

### 4F07h — Set/Get Display Start (required)

- I: AX=4F07h; BL = 00h set start (CX = first pixel, DX = first scan
  line) / 01h get / 80h set during vertical retrace (same CX/DX);
  02h/03h/82h/83h = schedule flips (byte offsets in ECX/EDX, for
  hardware triple buffering / stereo); 04h flip status; 05h/06h
  stereo enable/disable.
- 32-bit PM variant: DX:CX = byte offset / 4 in 8+ bpp modes
  (= the VGA CRTC start-address value); VBE 3.0 lets DX[15:14]
  carry the low two bits for pixel-perfect panning.
- O: AX = status; for get: CX/DX = position; for 04h: CX nonzero
  if flip done.
- This is the honest double/triple-buffer primitive:
  4F06h to widen the logical line + 4F07h/80h to flip at retrace.

### 4F08h — Set/Get DAC Palette Format (required)

- I: AX=4F08h; BL = 00h set / 01h get; BH = desired bits per primary
  (set only).
- O: AX = status; BH = actual bits per primary.
- DAC resets to 6 bits/primary on every mode set; the app must
  switch to 8 bits itself if wanted. Fails AH=03h in direct-color
  or YUV modes. Capability discoverable via 4F00h Capabilities D0.
- Get subfunction (01h) not supported via the 3.0 PM entry point.

### 4F09h — Set/Get Palette Data (required)

- I: AX=4F09h; BL = 00h set primary / 01h get primary /
  02h set secondary / 03h get secondary / 80h set during vertical
  retrace; CX = number of registers (≤ 256); DX = first register;
  ES:DI → table of 4-byte entries: **Blue, Green, Red, alignment
  byte** (BGRA order, 6 or 8 bits per primary per 4F08h).
- O: AX = status.
- Use this for non-VGA-compatible controllers (check
  ModeAttributes D5); VGA-compatible modes can program the DAC
  ports directly (§6). Get subfunctions are optional in 3.0 and not
  supported via the 3.0 PM entry point.

### 4F0Ah — Return VBE Protected Mode Interface (optional)

- I: AX=4F0Ah; BL=00h. O: AX = status; ES:DI → PM table;
  CX = table length (incl. code) for copying.
- Table: words at +00h/+02h/+04h = offsets of relocateable 32-bit
  near-callable routines for **Function 5 (set window), Function 7
  (set display start), Function 9 (set primary palette)**; +06h =
  offset of ports/memory subtable (0000h if none). See §9.

### 4F0Bh — Get/Set Pixel Clock (required)

- I: AX=4F0Bh; BL=00h get closest; ECX = requested Hz; DX = mode.
- O: AX = status; ECX = closest achievable clock (rounded up within
  1% tolerance rule). Used with CRTCInfoBlock/4F02h-D11 for custom
  refresh rates.

### 4F10h–4F15h — Supplemental specifications (optional)

Function numbers from AL=10h up are reserved for supplemental specs:
10h power management, 11h flat panel, 13h audio, 14h OEM,
**15h VBE/DDC (Display Data Channel)** — defers to the VBE/DDC
standard. In practice:

- **4F15h BL=00h — Report DDC capabilities**: I: AX=4F15h, BL=00h,
  CX=controller unit (00h primary). O: AX = status; BX = capabilities
  (DDC1/DDC2 support, transfer time, blank-during-transfer).
- **4F15h BL=01h — Read EDID**: I: AX=4F15h, BL=01h, CX=unit,
  DX=block number; ES:DI → 128-byte buffer. O: AX = status; buffer
  holds the EDID block.
- Real-world evidence: Linux `get-edid` performs exactly these two
  real-mode calls (`ax=0x4f15 bx=0x0` then `bx=0x1`)
  ([tonymacx86 get-edid log](https://www.tonymacx86.com/threads/another-edid-question.68213/));
  same call shapes appear in a QEMU/firmware EDID
  implementation ([aero commit](https://github.com/wilsonzlin/aero/commit/86096b7d3e8cc627618ab332c53934d357f6fd08)).
- Use case for us: ask the monitor for its EDID before choosing a
  VBE mode, rather than assuming 1024x768 exists. QEMU's VBE does
  not provide useful EDID [unverified] — treat EDID absence as
  normal, not an error.

## 4. VBE mode numbers

D0–D8 mode number (D8=1 ⟺ VESA-defined), D9–D12 reserved,
D11 refresh-rate-select, D12–D13 reserved, D14 linear-FB, D15
preserve-memory. Common VESA-defined graphics modes: 100h 640x400x256,
101h 640x480x256, 103h 800x600x256, 105h 1024x768x256, 10Eh 320x200x64K
(5:6:5), 111h 640x480x64K, 112h 640x480x16.8M, 114h 800x600x64K,
115h 800x600x16.8M, 117h 1024x768x64K, 118h 1024x768x16.8M.
Special mode 81FFh = "preserve memory + expose all video RAM".
Full table: [faqs.org supervga notes](http://www.faqs.org/faqs/pc-hardware-faq/supervga-programming/).

## 5. ModeInfoBlock layout (256 bytes)

Offsets computed from the VBE 3.0 struct field order (verified
against the spec struct listing):

| Offset | Size | Field | Notes |
|---|---|---|---|
| 0 | 2 | ModeAttributes | bitfield, §5.1 |
| 2 | 1 | WinAAttributes | D0 relocatable, D1 readable, D2 writeable |
| 3 | 1 | WinBAttributes | same |
| 4 | 2 | WinGranularity | window placement granularity, KB |
| 6 | 2 | WinSize | window size, KB (usually 64) |
| 8 | 2 | WinASegment | real-mode segment (<<4 = physical); 0 = no banked mode |
| 10 | 2 | WinBSegment | same |
| 12 | 4 | WinFuncPtr | real-mode far ptr to direct bank-switch routine |
| 16 | 2 | BytesPerScanLine | banked modes |
| 18 | 2 | XResolution | pixels (chars in text) |
| 20 | 2 | YResolution | pixels (chars in text) |
| 22 | 1 | XCharSize | cell width, px (nonzero-based) |
| 23 | 1 | YCharSize | cell height, px |
| 24 | 1 | NumberOfPlanes | 1 for packed-pixel |
| 25 | 1 | BitsPerPixel | total per pixel |
| 26 | 1 | NumberOfBanks | scanline banks (CGA legacy) |
| 27 | 1 | MemoryModel | 0 text, 1 CGA, 2 Herc, 3 planar, 4 packed-pixel, 5 nonchain-4, 6 direct color, 7 YUV |
| 28 | 1 | BankSize | KB per bank |
| 29 | 1 | NumberOfImagePages | (total − 1) |
| 30 | 1 | Reserved1 | always 1 in this spec revision |
| 31 | 1 | RedMaskSize | direct color / YUV only |
| 32 | 1 | RedFieldPosition | |
| 33 | 1 | GreenMaskSize | |
| 34 | 1 | GreenFieldPosition | |
| 35 | 1 | BlueMaskSize | |
| 36 | 1 | BlueFieldPosition | |
| 37 | 1 | RsvdMaskSize | |
| 38 | 1 | RsvdFieldPosition | |
| 39 | 1 | DirectColorModeInfo | D0 ramp programmable, D1 reserved field usable |
| 40 | 4 | **PhysBasePtr** | **linear framebuffer physical address** (0 = none) |
| 44 | 4 | Reserved2 | always 0 |
| 48 | 2 | Reserved3 | always 0 |
| 50 | 2 | LinBytesPerScanLine | for linear modes |
| 52 | 1 | BnkNumberOfImagePages | |
| 53 | 1 | LinNumberOfImagePages | |
| 54–61 | 8 | LinRedMaskSize…LinRsvdFieldPosition | linear-mode variants |
| 62 | 4 | MaxPixelClock | Hz |
| 66 | 189 | Reserved | zeroed |

### 5.1 ModeAttributes bits

D0 mode supported by current hw (must be 1); D1 reserved (=1);
D2 TTY BIOS functions supported; D3 1=color, 0=mono; D4 1=graphics,
0=text; D5 1=**not** VGA-compatible (0=compatible); D6 1=windowed
model **not** available; **D7 1=linear framebuffer available**;
D8 double-scan available; D9 interlaced; D10 hardware triple
buffering; D11 hardware stereo; D12 dual display start; D13–15 reserved.

Key pairs for us: for a real-mode boot use `ModeAttributes & 0x81`
supported+linear; check D7 then pass BX|0x4000 to 4F02h and map
PhysBasePtr.

## 6. VGA Mode 13h + DAC palette

- Set: `INT 10h, AH=00h, AL=13h`. 320x200, 256 colors, **linear
  chunky framebuffer** at segment 0xA000: offset `y*320+x`, bytes
  0–63,999. Details: [Mode 13h](https://en.wikipedia.org/wiki/Mode_13h).
- Palette: each of 256 entries is 18 bits (6 bits/channel). Not in
  memory — programmed via DAC ports:
  - `OUT 3C8h, index` — select palette index (0–255), resets the
    write cycle.
  - `OUT 3C9h, R` / `OUT 3C9h, G` / `OUT 3C9h, B` — 6-bit values
    (0–63), auto-advance to the next index after each R,G,B triple.
  - `OUT 3C7h, index` then read 3C9h thrice — read back a palette entry.
  - The 3C8 write must precede; writing R,G,B three times completes
    one entry. For a full palette, loop 256 entries: index once at
    3C8, then 768 `OUT 3C9h`.
  - Convention (QB Cult, verified QEMU-folk knowledge):
    `OUT &H3C8, col%` / `OUT &H3C9, r%` / `OUT &H3C9, g%` /
    `OUT &H3C9, b%` per entry; values 0–63.
    Source: [QB Cult "Understanding the VGA Palette"](http://www.petesqbsite.com/sections/tutorials/zines/qbcm/1-vga.html).
- VGA DAC details: writes of 3 R,G,B bytes per color, values
  0–63; default VGA palette is EGA-compatible in the first 16
  entries, then a gray ramp (16–31), then varied colors
  ([OSDev forum VGA palette note](https://forum.osdev.org/viewtopic.php?t=37227&p=308941)).
- 0xA0000–0xAFFFF must be marked reserved in our allocator (see
  GRAPHICS_COMPAT.md §2).

## 7. VGA text mode basics

- Buffer at physical **0xB8000** (segment 0xB800), 80x25 cells by
  default (mode 03h). Each cell = 2 bytes: **low byte = code point**,
  **high byte = attribute**. Source: [VGA text mode](https://en.wikipedia.org/wiki/VGA_text_mode).
- Attribute byte: bits 0–2 foreground R,G,B (bit 3 = intensity),
  bits 4–6 background R,G,B, bit 7 = blink (or 4th background bit
  if blink disabled — attribute-controller configurable).
  Colors 0–15: 0 black, 1 blue, 2 green, 3 cyan, 4 red, 5 magenta,
  6 brown, 7 light gray, 8 dark gray, 9 light blue, 10 light green,
  11 light cyan, 12 light red, 13 light magenta, 14 yellow, 15 white.
- 0xB8000–0xBFFFF also reserved in our allocator (text region
  shares the page with the graphics window).
- Font data lives in VGA plane 2 (not CPU-visible in text mode
  without sequencer reprogramming) — for our text console we use
  our own 8x8 font on top of the framebuffer instead (GRAPHICS.md),
  avoiding font-RAM hacking.

## 8. Bank switching for non-linear modes (code sketch)

Only needed if we ever touch a *windowed* (non-linear) VBE mode.
Our kernel's plan is Mode 13h (no banking: the whole 64 KB fits
one window) and VBE linear modes, but the mechanism is worth having
documented since QEMU's `-vga std` VBE BIOS does expose windowed
modes.

Pseudo-asm (16-bit real mode; mirrors the VBE 3.0 Appendix-2 sample
`setBank`, with DX scaled by WinGranularity):

```asm
; ES = 0xA000 (window segment from WinASegment)
; DX = bank number from MIB.WinGranularity math
;   dx_raw = (y * BytesPerScanLine + x) / (WinSize * 1024)
;   dx     = dx_raw * (WinSize / WinGranularity)   ; granularity units
; in: AX=4F05h, BH=00h (set), BL=window (0=A,1=B), DX=bank in granularity units
set_bank:
    cmp  dx, [cur_bank]
    je   .done            ; VBE sample-code optimization: skip if already set
    mov  [cur_bank], dx
    mov  ax, 4F05h
    mov  bx, 0000h        ; BH=0 set, BL=0 window A
    int  10h
    mov  bx, 0001h        ; BL=1 window B (set separately — some BIOSes split read/write windows)
    int  10h
.done:
    ret
```

Notes from the spec: set **both** Window A and Window B even if one
is nominally read/write — many BIOSes split them. `WinGranularity`
may be 4/16/64 KB (usually 64 KB = WinSize, making DX the raw bank
number). If `WinFuncPtr` is non-NULL, a direct far call with the
same BH/BL/DX avoids INT overhead (destroys AX/DX in ≤1.2 BIOSes).

## 9. Calling VBE from protected mode

VBE is a **real-mode** interface. Options, from simplest to hardest:

1. **Do all mode setup at boot in real mode** (our plan): the
   bootloader enumerates modes via 4F00h/4F01h, sets the chosen
   mode (13h via AH=00h, or 4F02h with BX|0x4000 for linear), and
   hands the kernel `PhysBasePtr` / aperture address. The kernel
   never calls VBE again — only the framebuffer + DAC ports.
   Runtime palette changes are DAC port writes, no BIOS needed.
2. **V8086 monitor**: run BIOS code in a VM86 task (protected-mode
   ring 0 → VM86). Community-standard approach
   ([OSDev Real Mode](https://osdev.wiki/wiki/Real_Mode));
   complexity lives in the VM86 interrupt/exception path.
3. **x86 emulator**: XFree86 `x86emu` (used by early Linux X to
   run VBE BIOSes) or `libx86emu` — emulates real mode only;
   "well-tested ... guaranteed to work with all SVGA cards"
   ([OSDev forum](https://forum.osdev.org/viewtopic.php?t=37241&start=15)).
   No ring transitions; biggest code lift.
4. **VBE 3.0 protected-mode entry point (4F0Ah interface +
   'PMID' block)**: optional — the spec says implementations
   *may not implement it* (VBE 3.0 "optional"). Where present:
   scan the first 32 KB of the video BIOS image (C0000h) for the
   `PMID` signature (checksum = 0), copy the 32 KB BIOS image to
   RAM, fill in selectors (BIOSDataSel→600h-byte zeroed block,
   A0000Sel/B0000Sel/B8000Sel, CodeSegSel, InProtectMode=1), call
   PMInitialize, then far-call the entry point exactly like INT 10h
   (AX=4Fxx in registers) with a dedicated 16-bit stack.
   Restrictions: no standard VGA BIOS functions via the PM entry
   point (VBE functions only), no extended text modes, and the
   **Get-style functions (4F03h, 4F07h-sub01, 4F08h-sub01,
   4F09h-get, 4F05h-sub01 get window, 4F06h-sub01 get) are not
   supported** — the OS must cache state itself. The 4F0Ah
   32-bit relocateable routines cover bank switch (5), display
   start set (7), palette set (9); ports/memory privilege info in
   the subtable (+06h); ES/DS selectors passed only if a memory
   location is listed.
   - Honest status: OSDev consensus is the PM entry point is
     *rarely actually implemented* ("If the protected mode
     interface actually worked in protected mode, which it
     typically won't" — [OSDev forum](https://forum.osdev.org/viewtopic.php?t=40763)).
     QEMU's VBE does not implement it. **Do not depend on it.**

Recommended: option 1 for the graphics stage. Options 2–3 are the
future bridge if runtime mode switching is ever needed.

## 10. Linear framebuffer mapping

- After 4F02h with BX|0x4000, read `PhysBasePtr` (ModeInfoBlock+40)
  from the 4F01h block. This is a **physical** address — must go
  through paging: create a write-combining-capable (PAT/MTRR
  `[unverified]` for our era — use plain UC or WB; P4 PAT exists,
  exact MTRR/PAT register indices not verified here) page mapping
  at a kernel virtual address. Until paging exists (early boot),
  identity-map it.
- Stride: use `LinBytesPerScanLine` (+50) for linear modes —
  **not** `XResolution * bpp/8`; padding is common.
- Depth handling: direct-color modes use the Lin* mask/field
  fields (+54–+61). 8-bpp packed-pixel modes use the DAC palette
  (as in Mode 13h). 24-bpp modes: pixel = 3 bytes (often padded
  to 4 in 32-bpp mode — the spec notes many GPUs prefer 32 bpp
  "merely for faster video memory access through 32-bit memory
  alignment" ([Wikipedia](https://en.wikipedia.org/wiki/VESA_BIOS_Extensions)).
- QEMU check: `-vga std` exposes VBE 3.0 with modes up through
  1600x1200 (per the Wikipedia mode table of VBox's VBE, a similar
  Bochs-derived implementation) — good for development.

## 11. Kernel-needed subset (graphics stage)

What the GorpOS graphics stage actually uses — and what it
explicitly does *not*:

**Uses (real-mode boot only):**
- `INT 10h AH=00h AL=13h` → Mode 13h, fb at 0xA0000.
- `INT 10h AX=4F00h/4F01h` → enumerate modes, read PhysBasePtr,
  stride, mask fields for the staged VBE linear mode.
- `INT 10h AX=4F02h BX=mode|0x4000` → set linear mode.
- DAC ports 3C8h/3C9h → `k_palette_set` (kernel-internal or
  direct OUTs; no BIOS needed at runtime).
- (optional) `INT 10h AX=4F07h BL=80h` page-flip in VBE modes;
  `INT 10h AX=4F09h` palette via BIOS for non-VGA-compatible
  hardware (rare in QEMU; skip unless needed).

**Defers:** 4F05h banking (no windowed modes in plan), 4F06h
logical line length, 4F04h save/restore, 4F08h 8-bit DAC (nice
fade helper — cheap, worth adding later), 4F15h EDID (probe once
at boot; non-fatal if absent).

**Does not use:** the 4F0Ah PM interface / 'PMID' entry point
(unreliable in the wild — do all VBE at boot), runtime mode
switching via VBE (no PM thunk planned in v1), CRTCInfoBlock
custom refresh (D11), stereo/triple-buffer subfunctions,
supplemental specs 10h/11h/13h/14h.

## Sources

- [VESA VBE Core Functions 3.0 (PDF)](https://git.guld-berg.dk/SingOS/SingOS/raw/branch/lets32bit/References/VESA_BIOS_Extensions_%28VBE%29_v3.pdf)
- [VBE 2.0 (PDF mirror)](http://www.phatcode.net/res/221/files/vbe20.pdf) / [vbe3.pdf mirror](http://www.petesqbsite.com/sections/tutorials/tuts/vbe3.pdf)
- [VESA BIOS Extensions — Wikipedia](https://en.wikipedia.org/wiki/VESA_BIOS_Extensions)
- [Mode 13h — Wikipedia](https://en.wikipedia.org/wiki/Mode_13h)
- [VGA text mode — Wikipedia](https://en.wikipedia.org/wiki/VGA_text_mode)
- [SuperVGA/VESA programmer's notes (faqs.org)](http://www.faqs.org/faqs/pc-hardware-faq/supervga-programming/)
- [VGA palette programming (QB Cult)](http://www.petesqbsite.com/sections/tutorials/zines/qbcm/1-vga.html)
- [OSDev Real Mode](https://osdev.wiki/wiki/Real_Mode)
- [OSDev forum — SVGA BIOS call in PM (x86emu)](https://forum.osdev.org/viewtopic.php?t=37241&start=15)
- [OSDev forum — VBE protected mode interface](https://forum.osdev.org/viewtopic.php?t=40763)
- [VGA text palette / DAC discussion (OSDev)](https://forum.osdev.org/viewtopic.php?t=37227&p=308941)
- [get-edid 4F15 usage log (tonymacx86)](https://www.tonymacx86.com/threads/another-edid-question.68213/)
- [aero QEMU/firmware EDID 4F15 implementation](https://github.com/wilsonzlin/aero/commit/86096b7d3e8cc627618ab332c53934d357f6fd08)

*Research: 6 search batches / Avenue 3; stopped with all planned
topics covered (4F00–4F0B, 4F15/DDC, ModeInfoBlock layout, DAC
palette, bank-switch sketch, PM calling, text mode).*
