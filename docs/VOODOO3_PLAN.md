# Voodoo3 (Avenger) 2D Bring-up Plan

Register-level plan for writing a GorpOS 2D driver for the **3dfx Voodoo3**
(chip codename **Avenger**). A driver author should be able to code from this
document alone.

**Conventions used in this document**

- `BAR0` = PCI base address register 0 (`memBaseAddr0`, PCI config `0x10`).
  All 2D offsets in this document are **byte offsets from BAR0** unless noted.
- Facts extracted directly from the primary datasheets are stated plainly.
- Facts that could not be verified against a primary source are marked
  **`[unverified]`**.
- **Do not copy Banshee assumptions onto Voodoo3.** Where the two chips
  differ, the difference is called out in `⚠ BANSHEE vs AVENGER` boxes.
  The Banshee 2D Databook is a useful conceptual reference, but its register
  encodings (especially the **status register**) do **not** apply verbatim.

**Primary sources** (all actually opened and mined; cite these, not
second-hand summaries):

1. *Voodoo3 High-Performance Graphics Engine for 3D Game Acceleration*,
   Programming Guide **rev 1.4a, 1999-06-14** — the main register reference.
   `https://cassini.mirrorservice.org/sites/www.bitsavers.org/components/3dfx/Voodoo3_Programming_Guide_r1.4a_199906.pdf`
2. *Avenger (a.k.a. Voodoo3) High Performance Graphics Engine*,
   Specification **rev 1.0, 1999-11-30** — later, more detailed document;
   resolves several ambiguities in the Programming Guide (command bits
   11:10/15:14, color packing tables, address space, VGA-when-not-primary).
   `https://bitsavers.pdp-11.net/components/3dfx/Voodoo3_Spec_r1.0_199911.pdf`
3. *Voodoo Banshee Universal Access 2D Databook*, rev 1.01 (1999-03-01) —
   engine concepts and PCI layout background only; **not** authoritative for
   Avenger encodings.
   `https://bitsavers.pdp-11.net/components/3dfx/Voodoo2_Banshee-2D_Databook_r1.0_199806.pdf`
4. `xf86-video-tdfx` (Xorg/XLibre 2D driver) — register defines
   (`src/tdfxdefs.h`) and FIFO/sync discipline (`src/tdfx_accel.c`):
   `https://github.com/x11libre/xf86-video-tdfx`
   (canonical upstream: `https://gitlab.freedesktop.org/xorg/driver/xf86-video-tdfx`).
   A community fork documents 3dfx's own PLL algorithm (pin m/k, sweep n):
   `https://github.com/sdz-mods/xf86-video-tdfx`.

---

## 1. Chip identification

| Item | Value |
|---|---|
| PCI vendor ID | `0x121A` (3Dfx Interactive) |
| PCI device ID (Voodoo3 / Avenger) | `0x0005` — "Speed Sorted" (fully programmable PLL) |
| PCI device ID (slower Avenger bin) | `0x0004` — "Not so fast" |
| Banshee (for comparison only) | `0x121A:0x0003` |

The device ID matters for clock programming: on device `0x0005` the
`pllCtrl1` M/N/K fields are fully programmable; on device `0x0004` the M
field of `pllCtrl1` is **forced** by hardware. The Programming Guide says it
is forced to `0x24`, limiting the graphics/memory clock to 141 MHz; the
Avenger Spec r1.0 says it is fixed to `0x18` (24 decimal) — **`[unverified]`:
the two official documents conflict; read `pllCtrl1` back after reset to see
the actual forced value on the card in hand.**

PCI class code depends on strapping pin VMI_HD4: `0x030000` (VGA compatible)
when low, `0x048000` (other multimedia device) when high.

---

## 2. PCI initialization

### 2.1 Configuration-space registers (Voodoo3-specific)

| PCI offset | Name | Notes |
|---|---|---|
| `0x00` | Device/Vendor ID | `0x0005_121A` or `0x0004_121A` |
| `0x04` | Status/Command | see §2.2 |
| `0x08` | Class code / revision | class per strapping (§1); rev `0x01`=.35µm, `0x02`=.25µm |
| `0x0C` | BIST/Header/Latency/Cache | single-function device; latency & cache-line read 0 |
| `0x10` | `memBaseAddr0` (BAR0) | 32-bit, non-prefetchable, **32 MiB** |
| `0x14` | `memBaseAddr1` (BAR1) | 32-bit, non-prefetchable; framebuffer aperture, size by strapping: 4/8/16 MiB (VMI_HD[6:5]) |
| `0x18` | `ioBaseAddr` (I/O BAR) | **256 bytes** of I/O space |
| `0x2C` | Subsystem vendor/ID | loaded from expansion ROM at init |
| `0x30` | Expansion ROM BAR | base + size by strapping (VMI_HD1; overridable via `miscInit1[25]`) |
| `0x34` | Capabilities pointer | AGP + ACPI caps |
| `0x3C` | Interrupt line / pin | |
| `0x40` | `fabID` | |
| `0x4C` | **Config-space status alias** | same bits as the I/O-space `status` register (§4); handy when MMIO isn't mapped yet |
| `0x50` | Config scratch | |
| `0x54`–`0x5C` | AGP capability/status/command | |
| `0x60`–`0x64` | ACPI capability/control | D0 and D3hot only |

### 2.2 PCI command register (`0x04`, low 16 bits)

- bit 0 — I/O-space enable: set to use the 256-byte I/O BAR.
- bit 1 — Memory-space enable: set to use BAR0/BAR1.
- bit 2 — **Bus-master enable: readonly 0.** Voodoo3 never acts as a PCI bus
  master. The 2D engine is fed entirely by host PIO writes into the PCI FIFO;
  **no bus mastering needs to be (or can be) enabled.** (Banshee is the same.)

### 2.3 BAR0 internal map (offsets from BAR0 base)

| Offset | Size | Function |
|---|---|---|
| `0x000000` | 512 KiB | I/O register remap (same registers as the I/O BAR; **except the VGA core set**) |
| `0x080000` | 512 KiB | CMD/AGP transfer / misc registers |
| `0x100000` | 1 MiB | **2D registers** (this document) |
| `0x200000` | 4 MiB | 3D registers |
| `0x600000` | 2 MiB | TMU0 texture download |
| `0x800000` | 2 MiB | TMU1 texture download |
| `0xA00000` | 2 MiB | ⚠ Programming Guide: FLASH/BIOS ROM access. Avenger Spec r1.0: **Reserved**. `[unverified]` — do not rely on either without probing. |
| `0xC00000` | 4 MiB | YUV planar space |
| `0x1000000` | 16 MiB | 3D linear framebuffer space |

BAR1 (`memBaseAddr1`) is the plain linear framebuffer aperture (4/8/16 MiB by
strapping). The first driver should draw through BAR1 for CPU access and use
BAR0+`0x100000` for the 2D engine.

### 2.4 I/O-space register map (offsets from I/O BAR base)

Same as the BAR0 remap at `+0x000000`. Key offsets: `0x00` status,
`0x04` pciInit0, `0x10` miscInit0, `0x14` miscInit1, `0x28` vgaInit0,
`0x2C` vgaInit1, `0x30` 2D command alias, `0x34` 2D srcBaseAddr alias
(both aliases exist to initialize SGRAM mode registers), `0x38` strapInfo,
`0x40`/`0x44`/`0x48` pllCtrl0/1/2, `0x4C` dacMode, `0x50` dacAddr,
`0x54` dacData, `0x5C` vidProcCfg, `0xB0`–`0xDF` VGA core registers
(**only in I/O space, never memory-mapped**), `0xE4` vidDesktopStartAddr,
`0xE8` vidDesktopOverlayStride.

---

## 3. MMIO access rules

- **All non-VGA register accesses must be 32-bit DWORD accesses.** No byte
  or word accesses to the 2D/init/video registers.
- VGA core registers keep their legacy byte-wide I/O semantics
  (ports `0x3B4/0x3D4`, `0x3C0`–`0x3CF`, `0x3B
...[truncated 26105 chars]- `0x100058` `srcSize` — source size, stretch blits only: bits 12:0 width, bits 28:16 height.
- `0x10005C` `srcXY` — signed 13-bit X (bits 12:0), signed 13-bit Y (bits 28:16).
  Screen-to-screen: first source pixel (corner per `command[15:14]`, §7.4).
  Host-to-screen: only X used — bit position (mono, bits 4:0) or byte position
  (color, bits 1:0) of the first pixel within the first launch DWORD.
- `0x100060` `colorBack` — background color for mono expansion / stipple.
- `0x100064` `colorFore` — fill color; foreground for mono expansion; ROP source stand-in for polygon fill.
- `0x100068` `dstSize` — width bits 12:0, height bits 28:16 (pixels).
- `0x10006C` `dstXY` — signed 13-bit X (bits 12:0), signed 13-bit Y (bits 28:16); first destination pixel (corner per `command[15:14]`).
- `0x100070` `command` — see §7.3.
- `0x100074`–`0x10007C` — **reserved: never write.**
- `0x100080`–`0x1000FC` — launch area: write-only trigger/data port.
- `0x100100`–`0x1001FC` — 64 color-pattern DWORDs (§7.7).

### 7.2 Format registers

`dstFormat` (`0x100014`): bits 13:0 = linear stride in **bytes** (not pixels);
bits 18:16 = destination format code:

| Code | Format |
|---|---|
| 1 | 8 bpp paletted |
| 2 | 15 bpp (`[unverified]` — implied by the color tables and the "8, 15, 16, 24, or 32" list; code-to-name mapping by elimination) |
| 3 | 16 bpp |
| 4 | 24 bpp packed |
| 5 | 32 bpp |

`srcFormat` (`0x100054`): bits 13:0 = source stride in bytes (or packing
override, see below); bits 19:16 = source format: 0 = 1-bpp mono, 1 = 8-bpp
paletted, 3 = 16-bpp RGB, 4 = 24-bpp RGB, 5 = 32-bpp RGB, 8 = YUYV422,
9 = UYVY422. Bit 20 = host-port byte swizzle, bit 21 = host-port word
swizzle (if both set, byte swizzle applies first). Bits 23:22 = source
packing: 0 = use stride field; 1 = byte-packed `ceil(w·bpp/8)`; 2 =
word-packed `ceil(w·bpp/16)*2`; 3 = dword-packed `ceil(w·bpp/32)*4`. Packed
source and tiled surfaces are mutually exclusive.

Color register packing (`colorFore`/`colorBack`, and pattern pixels):

| Format | Bit layout (MSB→LSB) |
|---|---|
| 8 bpp | `00000000_00000000_00000000_PPPPPPPP` |
| 15 bpp | `00000000_00000000_ARRRRRGG_GGGBBBBB` |
| 16 bpp | `00000000_00000000_RRRRRGGG_GGGBBBBB` |
| 24 bpp | `00000000_RRRRRRRR_GGGGGGGG_BBBBBBBB` |
| 32 bpp | `AAAAAAAA_RRRRRRRR_GGGGGGGG_BBBBBBBB` |

Format conversion on blits is automatic: 15/16→24/32 replicates MSBs into
the new LSBs; 24/32→15/16 drops LSBs; anything→32 zero-fills alpha; 8-bpp
destination accepts only 1-bpp (uses color registers) or 8-bpp sources.

### 7.3 `command` register (`0x100070`) — bit fields

| Bits | Meaning |
|---|---|
| 3:0 | Operation: 0=NOP (wait idle), 1=screen→screen blit, 2=screen→screen stretch, 3=host→screen blit, 4=host→screen stretch, 5=rectangle fill, 6=line, 7=polyline, 8=polygon fill, 13/14/15=SGRAM mode/mask/color writes |
| 7:4 | Reserved |
| 8 | **Initiate**: 1 = start when `command` is written; 0 = start on a launch-area write |
| 9 | Reversible lines |
| 10 | **After the blit/fill, `dst_x += dst_width`** |
| 11 | **After the blit/fill, `dst_y += dst_height`** |
| 12 | Stippled line |
| 13 | Pattern format: 1 = monochrome (pattern regs 0–1 = 8×8×1 bpp, expanded via colorFore/colorBack), 0 = color |
| 15:14 | **Traversal direction** (screen→screen only): bit 14 X (0=L→R, 1=R→L), bit 15 Y (0=T→B, 1=B→T); bit 15 also flips host→screen vertically |
| 16 | Transparent mono (1=transparent, 0=opaque): 0-bits in mono sources/patterns leave the destination unchanged vs writing colorBack |
| 19:17 | X pattern offset |
| 22:20 | Y pattern offset |
| 23 | Clip select: 0 = clip0 pair, 1 = clip1 pair |
| 31:24 | ROP0 (ternary ROP; `0xCC` = source copy) |

### 7.4 Screen-to-screen blit sequence (op = 1)

Copy `srcBaseAddr`+`srcXY` → `dstBaseAddr`+`dstXY`, size `dstSize`.
`srcSize` is **not** used. Source and destination may differ in format
(auto-conversion). Per the guide: "command[11:10] can be programmed to
automatically update dstXY at the end of each blt."

**Direction/corner selection** (Avenger Spec r1.0, verified):

| `command[15:14]` | Start corner (both srcXY and dstXY) | Use when |
|---|---|---|
| `00` | upper-left | no overlap, or dst above-left of src |
| `01` | upper-right | `dstX > srcX` (overlap) — start at `x+w-1` |
| `10` | lower-left | `dstY > srcY` (overlap) — start at `y+h-1` |
| `11` | lower-right | both overlap — start at `(x+w-1, y+h-1)` |

Direction bits apply to pure screen-to-screen blits only (not stretch);
with right-to-left blits, colorkeying and color conversion are unavailable.

Exact write sequence (bit 8 set, immediate):

```
  fifo_wait(n)
  dstBaseAddr, srcBaseAddr, dstFormat, srcFormat   // surface setup
  clip0Min = 0x00000000, clip0Max = pack(640, 480) // full-screen clip
  dstXY = start corner;  dstSize = pack(w, h)
  command = (0xCC << 24) | dir_bits | (1<<8) | 1    // ROP0=SRC_COPY, GO, op=1
  // engine runs; poll completion per §6 when the result must be visible
```

Launch variant: write `command` with bit 8 **clear**, then write the
source-XY DWORD to the launch area — that write both loads `srcXY` and
starts the blit. `rop` register only needed if colorkeying is on.

### 7.5 Rectangle fill sequence (op = 5)

Fill `dstBaseAddr`+`dstXY`, size `dstSize`, with `colorFore`, format
`dstFormat`. "Typically used to clear a buffer."

```
  fifo_wait(6)
  dstBaseAddr = fb_phys_byte_addr          // bit 31 = 0 (linear)
  dstFormat   = stride | (3 << 16)          // 16 bpp example
  colorFore   = packed fill color (§7.2 table)
  clip0Min    = 0x00000000
  clip0Max    = pack(SCREEN_W, SCREEN_H)
  dstXY       = pack(x, y)
  dstSize     = pack(w, h)
  command     = (0xCC << 24) | (1<<8) | 5   // = 0xCC000105: ROP0, GO, fill
```

Launch variant: `command = (0xCC<<24) | 5` (bit 8 clear), then write the
`(x,y)` DWORD to launch area `0x100080` — that write loads `dstXY` and
starts the fill. Bits 10/11 can auto-advance `dstXY` across repeated fills.

### 7.6 Host-to-screen blit sequence (op = 3) — worked example

From the Programming Guide §8.4.5 (1024×768×16 bpp text via mono bitmaps).
Registers with fixed meaning per blit; `srcSize` not used;
`srcBaseAddr` not used; send **exactly** the needed pixels.

Setup (once per format run):

```
  colorBack   = background (opaque text only)
  colorFore   = foreground
  dstXY       = first character position
  dstBaseAddr = primary surface address
  clip0Min    = 0x00000000
  clip0Max    = 0xFFFFFFFF        // clipping effectively off
  command     = 0xCC000003        // ROP0=0xCC SRC_COPY | op=3 host blit | bit8=GO
  dstFormat   = 0x00030800        // 16 bpp, stride 0x800 (2048)
  srcFormat   = 0x00400000        // 1-bpp mono, byte-packed
```

Per character (11×7 glyph example):

```
  dstSize = 0x0007000B            // 11 wide, 7 high
  srcXY   = 0x00000000            // source starts at lsb of first DWORD
  launch  = 0xC0608020            // rows 0-1 (packed mono bits)
  launch  = 0xC460C060            // rows 2-3
  launch  = 0x3B806EC0            // rows 4-5
  launch  = 0x00001100            // row 6
```

Notes: mono bit 0 = leftmost pixel; `srcXY` X selects the starting bit
(bits 4:0) or byte (bits 1:0) within the first launch DWORD; later rows
start at previous start + source stride. `commandExtra[2]` (wait-for-VSYNC)
**must not** be used with non-DMA host blits.

### 7.7 Patterns, lines, colorkeying (reference)

- 8×8 pattern at `0x100100`–`0x1001FC`, packed upper-left first,
  left-to-right then top-to-bottom; `pattern[0]` LSB = pixel (0,0). Register
  usage: mono → regs 0–1; 8 bpp → 0–15; 16 bpp → 0–31 (24/32 bpp `[unverified]`).
- Line: `bresError0/1` hold Bresenham terms; `lineStipple` + `lineStyle`
  control dashing; bit 9 makes A→B and B→A draw identical pixels.
- Colorkey test (per pixel): `pass = (min ≤ color ≤ max) && enable`,
  inclusive; per-channel for 16/24/32 bpp (alpha ignored at 32 bpp); direct
  index compare at 8 bpp; source colorkey unavailable for 1-bpp sources.
- ROP select: src-fail/dst-fail → ROP0 (`command[31:24]`); src-fail/dst-pass →
  ROP1 (`rop[7:0]`); src-pass/dst-fail → ROP2 (`rop[15:8]`); pass/pass → ROP3
  (`rop[23:16]`). With colorkeying off, ROP0 is used.

---

## 8. VGA / native modesetting

Strategy: **VGA text first (BIOS or legacy ports), then native CRTC/DAC
modeset to a linear 16/32-bpp mode.** Voodoo3 is "100% compatible VGA";
extended resolutions use the standard CRTC plus extension registers.

- Unlock protected CRTC registers: clear **CRTC index `0x11` bit 7**.
- Enable Avenger CRTC extensions (`0x1A`/`0x1B`): set
  **`vgaInit0[6]`** (`SST_VGA0_EXTENSIONS` in `tdfxdefs.h`).
- `vgaInit0[2]`: 0 = 6-bit DAC (VGA compatible); 1 = 8-bit.
- `vgaInit0[0]`: 1 disables the entire VGA core.
- CRTC `0x1A` (horizontal extension): bit 0 = horiz-total bit 8; bit 2 =
  display-end bit 8; bit 4 = blank-start bit 8; bit 5 = blank-end bit 6;
  bit 6 = retrace-start bit 8; bit 7 = retrace-end bit 5.
- CRTC `0x1B` (vertical extension): bit 0 = vert-total bit 10; bit 2 =
  display-end bit 10; bit 4 = blank-start bit 10; bit 6 = retrace-start bit 10.
- `dacMode[0]`: 1 = DAC 2:1 mode (2 pixels per clock) — **required above
  135 MHz**; then halve all horizontal timing and keep it divisible by 16.
- Pixel PLL: `pllCtrl0`, `fout = fref × (N+2) / ((M+2) × 2^K)`,
  `fref` = 14.31818 MHz, N = bits 15:8, M = bits 7:2, K = bits 1:0.
  Programming restrictions table (Programming Guide §14.4 Table 14.3) did not
  extract cleanly — **`[unverified]`**. 3dfx's own algorithm, per the
  community fork's notes: **pin M and K, sweep N, keep the VCO in a high
  stable band**. Worked 25.175 MHz example (640×480@60):
  N=107 (`0x6B`), M=29 (`0x1D`), K=1 → `pllCtrl0 = 0x6B75`,
  fout = 25.171 MHz (−0.016%) — **`[unverified]`**, validate on hardware
  (read back, compare against `CalcPLL`-style search).
- **Non-primary-VGA case** (Avenger Spec §5.2): power up with class
  `0x048000` (multimedia), set `vgaInit0[8]` (wakeup select → `0x3C3`), then
  set **I/O base + `0xC3` bit 0**; VGA registers are then reachable by
  stripping the leading `0x03` from the legacy port and adding the I/O base.
  In this mode legacy VGA **memory** is not accessible.
- Standard VGA 640×480@60 register values are public, well-known VGA
  constants (not Avenger-specific); program them through the VGA core after
  the PLL and extensions — **`[unverified]` on Avenger, validate on real
  hardware before freezing into the driver**:
  MISC=`0xE3`; SEQ=`03 01 0F 00 0E`; CRTC=`5F 4F 50 82 54 80 BF 1F 00 41 00
  00 00 00 00 00 9C 0E 8F 28 40 96 B9 A3 FF`; GFX=`00 00 00 00 00 40 05 0F
  FF`; ATTR=`00–0F palette, 41 00 0F 00 00`.
- `vidProcCfg[0]` = video processor enable; `vidProcCfg[27]` = hardware
  cursor enable; `vidDesktopStartAddr` = scanout base; `vidScreenSize`
  = visible dimensions.

**Firmware caveat (important):** when Voodoo3 is the primary display, the
x86 VGA BIOS normally initializes DRAM timings and clocks. On a secondary
card or without a BIOS, `dramInit0` (default `0x00579D29`) / `dramInit1`
must come from the expansion ROM's OEM configuration table — never invent
DRAM timings. Preserve firmware values unless you have the board's table.

---

## 9. Resets and recovery

- `miscInit0` (I/O `0x10`): bit 7 = VGA video timing reset; bit 6 = memory
  timing reset; **bit 5 = 2D graphics reset**; bit 4 = video timing reset;
  bit 1 = PCI FIFO + data packer reset (the guide's text mislabels this
  entry "7" — bit order 3,2,?,0 makes it bit 1); bit 0 = FBI graphics reset.
- `miscInit1[19]` = **command-stream reset**: set only when the chip is
  idle; unexecuted commands are discarded; **must be cleared after setting**.
  This is the recovery path for a hung 2D engine.
- After any reset, re-poll `status` (§4) and re-run the idle ritual (§6).

---

## 10. Interrupts

- `status[31]` = 1 → vertical-retrace interrupt pending; **any write to the
  status register clears it**.
- Enable via `pciInit0[18]` (PCI interrupt enable).
- The v1 driver should poll `status[6]` (retrace) instead; interrupts are
  optional/staged.

---

## 11. Bring-up checklist (staged, with pass criteria)

- [ ] **S0 — PCI probe.** Read config `0x00`; expect `0x0005121A` or
      `0x0004121A`. Read BAR0/BAR1 sizes by writing `0xFFFFFFFF` (expect
      32 MiB / 4–16 MiB). Read config `0x4C`; expect sane status bits.
      *Pass:* IDs match, BARs size as documented, `status[4:0]` reads `0x1F`
      (FIFO empty) after memory-space enable.
- [ ] **S1 — MMIO + FIFO.** Map BAR0; read `BAR0+0x100000` (2D status);
      confirm bit layout of §4. Write/read back a scratch-safe register
      (`clip0Min`). *Pass:* readback matches; FIFO free count decrements
      while a burst of writes is in flight.
- [ ] **S2 — Sync ritual.** Issue NOP + poll per §6; confirm `status[9]`
      and `status[10]` clear and free count returns to `0x1F`.
      *Pass:* `SST_IDLE()` returns 1 within a bounded loop; no hang.
- [ ] **S3 — VGA text preserved.** Do not break the firmware's text mode
      while probing. *Pass:* text screen still legible after S0–S2.
- [ ] **S4 — Native modeset (640×480×16).** Program PLL, unlock CRTC,
      enable extensions, load timings (§8). *Pass:* stable raster, no
      shimmer; `status[6]` toggles at ~60 Hz.
- [ ] **S5 — Solid fill.** Fill the full screen via §7.5; poll §6.
      *Pass:* uniform color; fill of a 100×100 sub-rect leaves the rest
      untouched (clip test); no streaking on repeated fills.
- [ ] **S6 — Screen-to-screen copy.** Copy a filled rect to a new location
      (§7.4), including an overlapping copy exercising direction bits
      (`01`/`10`/`11`). *Pass:* pixel-exact copy; overlapping copies show
      no tearing/smear.
- [ ] **S7 — Host-to-screen text.** Upload a mono glyph per §7.6.
      *Pass:* crisp glyphs, correct colors, auto-advance via bit 10 works.
- [ ] **S8 — Timeout + recovery.** Force a hang (e.g., fill with absurd
      size), confirm the watchdog fires, `miscInit1[19]` recovers, and S5
      passes again afterwards.

---

## 12. tdfx cross-reference

Upstream driver: `xorg/driver/xf86-video-tdfx`
(`https://gitlab.freedesktop.org/xorg/driver/xf86-video-tdfx`;
XLibre mirror `https://github.com/x11libre/xf86-video-tdfx`).

| This plan | tdfx location | Notes |
|---|---|---|
| 2D register offsets (`0x100000` block) | `src/tdfxdefs.h`: `SST_2D_OFFSET`, `SST_2D_COMMAND` (`+0x70`), `SST_2D_LAUNCH` (`+0x80`), `SST_2D_*` | Offsets match this plan exactly |
| command bit 8 | `SST_2D_GO BIT(8)` | |
| command bits 15:14 | `SST_2D_X_RIGHT_TO_LEFT BIT(14)`, `SST_2D_Y_BOTTOM_TO_TOP BIT(15)`; `BLIT_LEFT`, `BLIT_UP` | |
| NOP sync flush | `SST_2D_NOP` written to `SST_2D_COMMAND` | |
| 2D format coding | `TDFXSelectBuffer`: `stride \| (cpp+1)<<16` → 16 bpp = code 3, 32 bpp = code 5 | Matches §7.2 |
| FIFO accounting | `TDFXMakeRoomNoProp` in `src/tdfx_accel.c`: refill from `status & 0x1F` | 5-bit field — Avenger, not Banshee's 6-bit |
| Idle ritual | `TDFXSync` in `src/tdfx_accel.c`: NOP, poll `SST_BUSY` (bit 9) to 3 consecutive idle reads | Guide's `SST_IDLE()` wants `i > 3` (4 reads) |
| status bits | `SST_RETRACE BIT(6)`, `SST_FBI_BUSY BIT(7)`, `SST_BUSY BIT(9)` | |
| vgaInit0 bits | `SST_VGA0_EXTENSIONS BIT(6)`, `SST_VGA0_WAKEUP_SELECT_SHIFT 8`, `SST_VGA0_LEGACY_DECODE_SHIFT 9` | |
| PLL algorithm | `CalcPLL()` (upstream `tdfx_driver.c`): pin M/K, sweep N | Matches the community fork's description of 3dfx's own algorithm |

---

## 13. Gotchas (read before coding)

1. **Banshee status ≠ Avenger status.** Bits 4:0 + bit 5 busy on Avenger
   vs bits 5:0 on Banshee; retrace polarity inverted. Mask with `0x1F`,
   never `0x3F`.
2. **`0x1F` free = FIFO empty** (Avenger). `0x3F` is the Banshee value.
3. `status[9]` (device busy) is the sync bit — not bit 10 alone. The guide's
   ritual uses `SST_BUSY` and demands consecutive idle reads.
4. Reserved `0x100074`–`0x10007C`: never write.
5. `commandExtra[2]` (wait-for-VSYNC) gates the current command **and all
   following commands** until next vblank; never use it for host blits.
6. `command[11:10]` silently rewrites `dstXY` after each blit/fill — clear
   these bits unless auto-advance is wanted.
7. Overlapped screen-to-screen blits need direction bits + corner-correct
   `srcXY`/`dstXY`; right-to-left forbids colorkeying and format conversion.
8. `srcSize` is only for stretch blits; plain blits use `dstSize` for both.
9. All 2D MMIO must be DWORD; VGA stays byte-oriented.
10. Writing `status` clears the pending PCI interrupt — never use it as a
    scratch write.
11. No bus mastering: don't set PCI command bit 2 (readonly 0); the engine
    is PIO-fed.
12. PLL Table 14.3 restrictions and the device-`0x0004` forced-M value
    conflict between the two official docs — read back `pllCtrl1` on the
    actual card.
13. Preserve firmware `dramInit*`/clock values unless the board's ROM table
    is available; never invent DRAM timings.

## 14. Explicitly `[unverified]` items

- PLL Table 14.3 restriction values (extraction garbled).
- Device `0x0004` forced `pllCtrl1` M: `0x24` (Guide) vs `0x18`/24d (Spec).
- BAR0+`0xA00000` ROM aperture: FLASH (Guide) vs Reserved (Spec r1.0).
- `dstFormat` code 2 = 15 bpp (strongly implied, by elimination).
- 24/32-bpp pattern register counts.
- Worked `pllCtrl0 = 0x6B75` for 25.175 MHz and the 640×480 VGA register
  table — standard values, not yet validated on Avenger hardware.
- YUV overlay path (`vidProcCfg`, overlay registers) — out of scope for v1.
