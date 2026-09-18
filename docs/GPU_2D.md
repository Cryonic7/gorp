# 2D GPU Register-Level Research (GorpOS GPU shortlist)

Scope: what exists, in public, for programming the four GPU families in
`docs/GPU_SUPPORT.md` at the register level — **modesetting without
VBE/VBIOS** and the **2D acceleration engine**. Not a driver design;
a documentation verdict plus the concrete PCI IDs and register-model
facts a first driver would stand on. All claims come from the web
results cited, not from memory; anything unverifiable is marked
`[unverified]`.

Related: `docs/GPU_SUPPORT.md` (family shortlist), `docs/GRAPHICS.md`
(stage plan), `docs/VBE_REF.md` (the VBE/VGA fallback all of this
replaces).

## Verdict summary

| Family | Register-level docs (public) | 2D engine docs | Modeset docs | Verdict |
|---|---|---|---|---|
| Intel 8xx/9xx (845G–945G) | Datasheets (PCI config only); no public PRM for this era | Source-only (xf86-video-intel / i915) | Source-only | Hardware-rich, docs-poor |
| ATI Radeon R100–R500 | AMD-released 3D register guides (R300/R500); no public 2D guide | Source-only (xf86-video-ati, radeon DRM) | Source-only | Complex; 2D via driver source |
| Matrox G200–G550 | **Full public spec for G200** (register-level) | G200 spec; G400+ source-only (mga driver) | G200 spec | Best 2D quality; hardware rare |
| 3dfx Voodoo3 | **Full public specs + dedicated 2D databook** | 2D databook, register-level | VGA-compatible | **Best documented — first 2D driver candidate** |

## 1. Intel 8xx/9xx (Extreme Graphics, GMA 900/950)

### Documentation verdict
Intel publishes open PRMs for later generations (e.g. the open-source
IHD PRM set for Ironlake/HD Graphics:
[Vol 1 Part 1](https://cdrdv2-public.intel.com/690951/ilk-ihd-os-vol1-part1r2.pdf)),
but **no public Programmer's Reference Manual for the 8xx/9xx era
surfaced** in 13 search batches. What is public:
- **GMCH datasheets** with the full Integrated Graphics Device PCI
  config space: [82865G GMCH Datasheet](https://mirrors.pdp-11.net/_x86/download.intel.com/design/chipsets/datashts/25251405.pdf),
  [82845G GMCH datasheet](http://www.ic-on-line.net/download.php?id=1530729&pdfid=D7F697C3518F5CA70DBD00B8BA582A30&file=0266\845g_351097.pdf),
  [945 family datasheet](https://media.ic-find.com/datasheets/7ef528a9e078e69f646d1b647f9a813b.pdf).
- The **open-source drivers are the programming reference**:
  `xf86-video-intel` (the [intel(4) manpage](https://manpages.ubuntu.com/manpages/focal/man4/intel.4.html)
  lists 830M/845G/852GM/855GM/865G/915G/915GM/945G/945GM/965G with
  hardware modesetting) and the kernel **i915** DRM driver, which
  supports the i845, i865, i915, i945, i965 and G33 series
  ([i915(7d)](https://docs.oracle.com/cd/E19963-01/html/821-1475/i915-7d.html)).

### PCI IDs (Integrated Graphics Device = Device 2, Function 0)
| Part | DID2 | Source |
|---|---|---|
| 82845G (Extreme Graphics) | 8086:2562 | 82845G GMCH datasheet, DID2=2562h |
| 82865G (Extreme Graphics 2) | 8086:2572 | 82865G GMCH datasheet, DID2=2572h |
| 82915G (GMA 900) | 8086:2582 [unverified] | — |
| 82945G (GMA 950) | 8086:2772 | 945 datasheet, DID2=2772h |

The 865G datasheet confirms the layout details a driver needs: Device 2
exposes **MMADR** (memory-mapped register range), **GMADR** (graphics
aperture), and an **IOBAR**; Function 0 "can be VGA compatible or not",
selected by bit 1 of the GC register (Device 0, offset 52h).

### Modesetting approach
Keep VBIOS/VBE as stage 1 (`docs/VBE_REF.md`). Native modeset (stage 2)
means programming the two display pipes, CRTC timing registers, and the
analog PLL in MMIO space — all only documented in the driver sources
(kernel `drivers/gpu/drm/i915`, `xf86-video-intel`). No public register
spec for the 8xx/9xx display engine exists to cite.

### 2D engine
- GMA 900: "256-bit 2-D engine" @ 333 MHz core
  ([D915GMH technical product specification](https://theretroweb.com/motherboard/manual/d915gmh-technical-product-specification-640f98e4b29b6903494078.pdf)).
- GMA 950: "optimized 256-bit BLT engine"
  ([D945GNT technical product specification](https://www.scribd.com/document/96578137/Mother-Board-INTEL-D945GNT-TechProdSpec)).
- Programming model: BLT (bitblt/fill) commands submitted through the
  ring/batchbuffer to the 2D engine — **source-only knowledge** for
  this generation (the i915 kernel driver's blitter code). There is no
  public "write register X to blit" document for 8xx/9xx.

## 2. ATI Radeon R100–R500

### Documentation verdict
AMD's 2008 open-documentation push released, without NDA:
- **R300 3D register reference guide** (99 pages; color buffer, fog,
  geometry, rasterizer, texture, vertex, Z)
  ([announcement](https://www.phoronix.com/review/amd_r300_guide)).
- **R500 3D programming documentation** (register reference +
  programming guide covering command processor, vertex/fragment
  shaders) ([R5xx Acceleration v1.5](http://www.x.org/docs/AMD/old/R5xx_Acceleration_v1.5.pdf)).
- Production **microcode for R100–R600** into the DRM tree
  ([announcement](https://www.phoronix.com/review/amd_microcode)).
- A commitment to re-release **R100/R200 specifications**
  ([Phoronix](https://www.phoronix.com/news/NjIwMg)).
But these are **3D** documents. No public 2D programmer's guide was
found; the 2D engine is documented only through the open drivers.

### 2D engine
Radeon's 2D engine descends from the RAGE GUI-master engine, whose
register-level programming *is* public and illustrative:
[RAGE PRO and Derivatives Programmer's Guide (2000)](https://pabjggh.vgamuseum.info/images/doc/ati/rage/prg-215r3-00-10_rage_pro_and_derivatives_programmers_guide_mar00.pdf)
shows the draw-op sequence: `DP_SRC`/`DP_MIX`/`DP_PIX_WIDTH` context,
`SRC_OFF_PITCH`/`SRC_CNTL`/`SRC_Y_X`, then `DST_OFF_PITCH`,
`DST_Y_X`, `DST_HEIGHT_WIDTH` to kick a blit. On R100–R500 the same
engine is driven through the **PM4 command processor**; the R5xx
acceleration guide documents the synchronization side: wait for
`2D_IDELCLEAN`/`3D_IDELCLEAN`, flip the display by writing the CRTC
base address via type-0 packets and polling `WAIT_UNTIL` for
`WAIT_CRTC_PFLIP`/`WAIT_BOTH_CRTC_PFLIP`. The Linux driver is the
register map:
[`drivers/gpu/drm/radeon/r100.c`](https://cgit.freedesktop.org/~nh/linux/tree/drivers/gpu/drm/radeon/r100.c?id=ff611c41c2223ed743395136ea450ed63e1576a3)
uses `RADEON_CRTC_GEN_CNTL` / `RADEON_CRTC2_GEN_CNTL` for the two
CRTCs and `RADEON_RBBM_STATUS` (bit `RADEON_RBBM_ACTIVE`) as the
2D/GUI idle test. The
[x.org Radeon feature matrix](https://www.x.org/wiki/RadeonFeatureUMS/)
records XAA/EXA 2D acceleration and modesetting DONE for every chip
from R100 through R500.

### Modesetting approach
Two CRTCs, reference-clock PLLs, and DAC/LVDS/TMDS outputs; hotplug
sense via `FP_GEN_CNTL` bits (see `r100.c`). Modesetting is the most
complex of the four families (dual-head plumbing, PLL post-dividers).
Reference implementation: `xf86-video-ati` + radeon KMS. This is the
family to attempt only after a simpler first driver works.

### Chips covered (per [OpenBSD radeon(4)](https://man.openbsd.org/OpenBSD-5.6/radeon.4))
R100 7200 · RV100 7000 · RV200 7500 · R200 8500/9100 · RV250 9000 ·
RV280 9200/9250 · RS300/RS350/RS400/RS480 IGPs · R300 9700/9500 ·
R350 9800 · R360 9800XT · RV350 9600/9550 · RV360 9600XT · RV370 X300 ·
RV380 X600 · RV410 X700 · R420/R423/R430 X800.
PCI vendor is 1002h (ATI); per-part device IDs were **not verified**
in this pass — look them up in `pci.ids`/kernel tables, don't guess.

## 3. Matrox G200–G550

### Documentation verdict
- **G200: a full public register-level specification exists**:
  [MGA-G200 Specification, Nov 1998](https://bitsavers.pdp-11.net/components/matrox/_dataSheets/MGA-G200_199811.pdf)
  ([mirror](http://iommu.com/datasheets/video/matrox/MGA-G200_199811.pdf)).
  The crawled text shows genuine register-level depth (e.g. DAC
  registers §3-327 `XMISCCTRL`, bit fields `dacpdN`, `mfcsel`,
  `vga8dac`, `ramcs`, `vdoutsel`).
- G400/G450/G550: no public register documents surfaced; the
  reference is the open-source **mga** X driver. Matrox's reputation
  for best-in-class 2D image quality makes this family attractive,
  but hardware is rarer and pricier on the retro market than the
  others.

### PCI IDs (verified via [DeviceHunt](https://devicehunt.com/view/type/pci/vendor/102B/device/0538))
| Part | ID |
|---|---|
| MGA G200 (PCI) | 102B:0520 |
| MGA G200 (AGP) | 102B:0521 |
| MGA G400 / G450 | 102B:0525 |
| MGA G550 | 102B:2527 [unverified] |

### Modesetting approach
CRTC programming through Matrox's extended CRTC registers plus the
on-chip PLL for the pixel clock — all documented for G200 in the spec
above. Bring-up path: standard VGA for text, then native CRTC/DAC
programming for the framebuffer.

### 2D engine
Matrox's drawing engine (DWG registers) — historically the 2D-quality
reference. The G200 spec documents it; for G400/G450/G550 the engine
is programmed per the mga driver source. The G450 adds DualHead
(two CRTCs).

## 4. 3dfx Voodoo3 — the documentation standout

### Documentation verdict
3dfx is the only vendor of the four with **complete, public,
register-level documentation including a dedicated 2D databook**:
- [Voodoo3 Programming Guide r1.4a (Jun 1999)](https://cassini.mirrorservice.org/sites/www.bitsavers.org/components/3dfx/Voodoo3_Programming_Guide_r1.4a_199906.pdf)
- [Voodoo3 Specification r1.0 (Nov 1999)](https://bitsavers.pdp-11.net/components/3dfx/Voodoo3_Spec_r1.0_199911.pdf)
- [Voodoo3 Data Book r1.0 (Aug 1999)](http://kuiper.mirrorservice.org/sites/www.bitsavers.org/components/3dfx/Voodoo3_Data_Book_r1.0_199908.pdf)
- [Voodoo Banshee Universal Access **2D Databook** r1.01 (Mar 1999)](https://bitsavers.pdp-11.net/components/3dfx/Voodoo2_Banshee-2D_Databook_r1.0_199806.pdf)
  — register-level 2D detail incl. PCI config registers
- [Voodoo Graphics Specification](http://www.o3one.org/hwdocs/video/voodoo_graphics.pdf) (SST-1)
- 3dfx **open-sourced Glide and the Voodoo 2/3 specs** in Dec 1999
  ([The Register](https://www.theregister.com/1999/12/07/3dfx_open_sources_glide_voodoo/));
  the **2D acceleration hardware spec** was released even earlier that
  year. The Linux **tdfx** driver is open source.

### PCI IDs
| Part | ID | Source |
|---|---|---|
| 3dfx (vendor) | 121Ah | Banshee 2D Databook: "3Dfx Interactive Vendor Identification. Default is 0x121a" |
| Voodoo Banshee | 121A:0003 | Banshee 2D Databook: "Banshee Device Identification. Default is 0x3" |
| Voodoo3 "Avenger" | 121A:0005 | Everest report on a Voodoo3 1000 AGP ([computerhope](https://www.computerhope.com/forum/index.php?topic=17072.0)) |

### Modesetting approach
Voodoo3 keeps a **fully VGA-compatible core**: standard VGA
CRTC/DAC/sequencer registers set the mode, so text-mode and basic
modesetting work with plain VGA programming (`docs/VBE_REF.md` §5/§7).
The Data Book lists a 350 MHz RAMDAC (Voodoo3 3000/3500) and the chip
is "essentially a Voodoo2 with an integrated 128-bit 2D accelerator"
([Wikipedia](https://en.wikipedia.org/wiki/Voodoo3)).

### 2D engine
"Fully featured 128-bit BitBlt Engine: Windows GDI in hardware"
(Voodoo3 Data Book §2.2.2). The programming model is a flat
memory-mapped register file plus a **command FIFO**: the status
register exposes PCI FIFO freespace (bits 5:0, 0x3f = empty), vertical
retrace (bit 6), and FBI graphics-engine busy (bit 7). The spec
documents per-register **sync** (flush pipeline before load) and
**FIFO** (write pushed into the PCI FIFO) semantics, so the driver
knows exactly which writes need fences. That is a simpler model than
Intel's ring/blitter or ATI's PM4 packets.

## Ranked recommendation: which to drive first

1. **3dfx Voodoo3 — first 2D-accelerated driver.** Complete public
   register specs, a dedicated 2D databook, VGA-compatible bring-up,
   the simplest submission model (FIFO + status bits), an open-source
   tdfx driver to cross-check against, and cheap plentiful hardware
   (PCI and AGP cards). Lowest documentation risk of the four.
2. **Intel 865G/945G — first integrated-graphics driver.** The
   hardware is everywhere and datasheets give the PCI-level facts
   (BARs, DID2, VGA-compat bit), but native modeset and the 256-bit
   BLT engine are source-only — expect to read the i915 kernel
   driver as the spec.
3. **Matrox G200 — the 2D-quality pick.** Full public G200 spec;
   ideal if a G200 is on hand, but G400+ are source-only and cards
   are harder to find.
4. **ATI Radeon R100–R500 — later.** Most complex modesetting
   (dual CRTC + PLLs), 2D only documented via driver source despite
   the public 3D docs. Attempt after Voodoo3 proves the driver
   architecture.

Staged plan stays: VBE/VBIOS framebuffer (now) → VGA-native text →
Voodoo3 2D driver → Intel iGPU driver → Radeon/Matrox as hardware
allows.

*Research: 13 search batches / Avenue 4; stopped after a full batch
with effectively no novel material (>98% non-novel — driver-download
and config-file pages only).*
