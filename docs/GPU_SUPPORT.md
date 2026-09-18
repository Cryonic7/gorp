# GorpOS — GPU Support Research (Top 100)

**Method:** "Most popular" = market presence from the 3D-accelerator era
(1996) through the Vista era (~2007), plus a small forward-path set
(post-2007) so the real-driver epic (FULL_SCOPE Layer 5, Phase 10) knows
what it grows into. Assessment is relative to *our* graphics staging:
VGA Mode 13h → VESA VBE linear framebuffer → software GL subset → real
GPU driver program → Vulkan (research).

**Assessment values:**
- `framebuffer-only` — usable through VGA/VESA only (2D/3D engine
  undocumented or the card is a 3D-only add-in).
- `VESA-ok` — VBE linear framebuffer works; no acceleration expected.
- `2D-possible` — public docs or a working open driver exist for 2D
  accel/modesetting; a bounded driver effort could succeed.
- `3D-epic` — 3D engine needs the full real-driver epic (command
  submission, shader compiler); no shortcut exists.
- `blocked-needs-firmware-or-docs` — no public programming docs and/or
  signed firmware required; not actionable without vendor cooperation.

**The two facts that decide most rows:** (1) Intel iGPUs and pre-R600
Radeons have fully open stacks (no firmware, documented modesetting) —
they are the only sane Phase 10 targets. (2) NVIDIA needs no firmware
before Maxwell (GTX 900), but never published docs — Nouveau is
reverse-engineered and thin on P4-era cards. (3) Everything obscure
(XGI, SiS Xabre, DeltaChrome, Kyro) is documentation-blocked.

**Verification note:** per-item specs (year, bus, VRAM, DX/OGL levels)
compiled from general references (TechPowerUp GPU database, Wikipedia)
and individually `[unverified]` unless cited. Driver/firmware/docs
claims carry citations. Research: 6 batches / 17 searches; stopped per
the >98%-non-novel rule.

---

## Tier A — Works with our VGA/VESA stage (framebuffer-only / VESA-ok)

No driver needed: VGA BIOS + VBE INT 10h.

| # | GPU | Vendor | Year | Bus | VRAM | APIs | Assessment | Rationale |
|---|-----|--------|------|-----|------|------|------------|-----------|
| 1 | Voodoo Graphics (Voodoo1) | 3dfx | 1996 | PCI | 4 MB | Glide | framebuffer-only | 3D-only add-in card; 2D via passthrough cable. |
| 2 | Voodoo2 | 3dfx | 1998 | PCI | 8/12 MB | Glide | framebuffer-only | Same passthrough architecture. |
| 3 | ET4000 | Tseng Labs | 1989 | ISA | 1–2 MB | — | framebuffer-only | Pre-3D VGA workhorse; VESA via BIOS. |
| 4 | ET6000 | Tseng Labs | 1996 | PCI | 2 MB | — | framebuffer-only | Fast 2D of its day; no docs need. |
| 5 | GD5446 | Cirrus Logic | 1995 | PCI | 2–4 MB | — | framebuffer-only | Ubiquitous OEM 2D; VESA fine. |
| 6 | GD5465 (Laguna3D) | Cirrus Logic | 1997 | AGP | 4–8 MB | DX5 | VESA-ok | 3D engine obscure; treat as VESA. |
| 7 | Trio64 | S3 | 1995 | PCI | 1–2 MB | — | framebuffer-only | The default 90s 2D chip. |
| 8 | ViRGE | S3 | 1995 | PCI | 2–4 MB | DX5 | VESA-ok | "3D decelerator"; VESA is the honest path. |
| 9 | Savage 3D | S3 | 1998 | AGP | 8/16 MB | DX6 | VESA-ok | XFree86 3.3.6-era driver existed; VESA safer. |
| 10 | Savage4 | S3 | 1999 | AGP | 16/32 MB | DX6 | VESA-ok | Same. |
| 11 | Savage 2000 | S3 | 1999 | AGP | 32/64 MB | DX7 | VESA-ok | T&L chip, drivers were broken; VESA. |
| 12 | ProSavage (KM133) | S3/VIA | 2000 | IGP | shared | DX6 | VESA-ok | Integrated; VESA. |
| 13 | TGUI 9680 | Trident | 1996 | PCI | 2–4 MB | — | framebuffer-only | OEM 2D. |
| 14 | CyberBlade | Trident | 1999 | IGP | shared | DX6 | VESA-ok | Laptop IGP. |
| 15 | Blade XP | Trident | 2001 | AGP | 32 MB | DX8 | VESA-ok | Obscure; VESA. |
| 16 | SiS 6326 | SiS | 1997 | AGP/PCI | 4–8 MB | DX5 | VESA-ok | DVD-decoder fame; VESA. |
| 17 | SiS 530 | SiS | 1998 | IGP | shared | DX6 | VESA-ok | Socket 7 IGP. |
| 18 | Imagine 128 | Number Nine | 1996 | PCI | 4–8 MB | — | VESA-ok | Good 2D; no 3D worth chasing. |
| 19 | Ticket to Ride IV | Number Nine | 1998 | AGP | 32 MB | DX6 | VESA-ok | 128-bit 2D monster; VESA. |
| 20 | Vérité V1000 | Rendition | 1996 | PCI | 4 MB | proprietary | VESA-ok | Quake-era; docs never opened. |
| 21 | Vérité V2200 | Rendition | 1998 | AGP | 8 MB | DX6 | VESA-ok | Same. |
| 22 | Permedia 2 | 3Dlabs | 1997 | AGP | 8 MB | OGL 1.1 | VESA-ok | Workstation 2D/3D; VESA. |
| 23 | PCX2 | PowerVR (NEC) | 1997 | PCI | 4–8 MB | PowerSGL | VESA-ok | Tile renderer; Apocalypse/Matrox m3D. |
| 24 | Neon 250 | PowerVR | 1999 | AGP | 16/32 MB | DX6 | VESA-ok | Tile-based; no open docs. |
| 25 | Kyro (STG4000) | STMicro/PowerVR | 2000 | AGP | 32/64 MB | DX6 | VESA-ok | TBDR; Linux drivers were *announced* but TBDR is hostile to a naive driver. |
| 26 | Kyro II (STG4500) | STMicro/PowerVR | 2001 | AGP | 32/64 MB | DX7 | VESA-ok | Same; surprisingly competitive with GeForce2 GTS. |
| 27 | Volari V3 | XGI | 2003 | AGP | 64/128 MB | DX8.1 | blocked-needs-firmware-or-docs | No public docs; drivers were notoriously bad. |
| 28 | Volari V8 (XG40) | XGI | 2003 | AGP 8x | 256 MB | DX9.0/OGL1.5 | blocked-needs-firmware-or-docs | Specs verified (130 nm, 4ps/2vs); zero open documentation. |
| 29 | Volari Duo V8 Ultra | XGI | 2004 | AGP 8x | 2×256 MB | DX9.0 | blocked-needs-firmware-or-docs | Dual-GPU; same docs situation, worse. |
| 30 | Xabre 400 | SiS | 2002 | AGP 8x | 64/128 MB | DX8.1 | blocked-needs-firmware-or-docs | Became the XGI Volari lineage; no docs. |
| 31 | DeltaChrome S8 | S3/VIA | 2004 | AGP 8x | 128 MB | DX9.0 | blocked-needs-firmware-or-docs | Last gasp of S3; no docs. |
| 32 | UniChrome (CLE266) | VIA | 2002 | IGP | shared | DX7 | VESA-ok | OpenChrome driver exists but unmaintained. |

Kyro/Kyro II years and TBDR notes:
[PC Gamer history](https://www.pcgamer.com/from-voodoo-to-geforce-the-awesome-history-of-3d-graphics/6/),
[Wikipedia/PowerVR](https://en.wikipedia.org/wiki/PowerVR).
XGI Volari V8 specs (Sep 2003, AGP 8x, DX9.0, OGL 1.5):
[TechPowerUp](https://www.techpowerup.com/gpu-specs/volari-v8.c85).
VIA OpenChrome unmaintained:
[linux-gaming driver survey](https://linux-gaming.kwindu.eu/index.php?title=Graphic_drivers_on_Linux).

## Tier B — Real driver work is tractable (2D-possible)

Public specs, vendor-supported open drivers, or mature reverse-engineered
drivers exist. These are the only GPUs worth a native driver before the
3D epic.

| # | GPU | Vendor | Year | Bus | VRAM | APIs | Assessment | Rationale |
|---|-----|--------|------|-----|------|------|------------|-----------|
| 33 | Voodoo Banshee | 3dfx | 1998 | AGP/PCI | 16 MB | Glide/DX6 | 2D-possible | 3dfx open-sourced Glide and Voodoo 2/3 specs (Dec 1999); tdfx DRI driver existed. |
| 34 | Voodoo3 | 3dfx | 1999 | AGP | 16 MB | DX6/OGL1.1 | 2D-possible | Same open specs; best-documented 3D target of the era. |
| 35 | Voodoo4 4500 | 3dfx | 2000 | AGP | 32 MB | DX6 | 2D-possible | Same family. |
| 36 | Voodoo5 5500 | 3dfx | 2000 | AGP | 64 MB | DX6/OGL1.1 | 2D-possible | Dual-chip; same family. |
| 37 | Millennium | Matrox | 1995 | PCI | 2–8 MB | — | 2D-possible | Matrox released 2D specs and backed XFree86 work. |
| 38 | Millennium II | Matrox | 1997 | PCI | 4–16 MB | — | 2D-possible | Same. |
| 39 | Mystique | Matrox | 1996 | PCI | 2–8 MB | DX5 | 2D-possible | Same. |
| 40 | G200 | Matrox | 1998 | AGP | 8/16 MB | DX6 | 2D-possible | Open 2D specs published. |
| 41 | G400 | Matrox | 1999 | AGP | 16/32 MB | DX6/OGL1.2 | 2D-possible | G400 DRI 3D existed; 2D is the bounded target. |
| 42 | G450 | Matrox | 2000 | AGP | 16/32 MB | DX6 | 2D-possible | Same family. |
| 43 | G550 | Matrox | 2001 | AGP | 32 MB | DX8 | 2D-possible | Same family. |
| 44 | i740 | Intel | 1998 | AGP | 8 MB | DX6 | 2D-possible | Open X driver; no firmware. |
| 45 | 810 (i810) | Intel | 1999 | IGP | shared | DX6 | 2D-possible | Unified memory; needs GART-style allocation in our driver model. |
| 46 | 815 (i815) | Intel | 2000 | IGP | shared | DX6 | 2D-possible | Same. |
| 47 | 845G | Intel | 2002 | IGP | shared | DX7 | 2D-possible | Same. |
| 48 | 852GM/855GM | Intel | 2003 | IGP | shared | DX7 | 2D-possible | Same. |
| 49 | 865G | Intel | 2003 | IGP | shared | DX7 | 2D-possible | Same. |
| 50 | 915G (GMA 900) | Intel | 2004 | IGP | shared | DX9 | 2D-possible | i915 DRM family; fully open. |
| 51 | 945G (GMA 950) | Intel | 2005 | IGP | shared | DX9 | 2D-possible | Same. |
| 52 | GMA 3000 (946GZ) | Intel | 2006 | IGP | shared | DX9 | 2D-possible | Same. |
| 53 | GMA X3000 (G965) | Intel | 2006 | IGP | shared | DX9/OGL1.5 | 2D-possible | Same. |
| 54 | Rage II | ATI | 1996 | PCI | 2–8 MB | DX5 | 2D-possible | mach64 X driver. |
| 55 | Rage Pro | ATI | 1997 | AGP | 8 MB | DX5 | 2D-possible | Same. |
| 56 | Rage 128 | ATI | 1998 | AGP | 16/32 MB | DX6/OGL1.2 | 2D-possible | r128 DRI 3D existed. |
| 57 | Radeon 7000 (RV100) | ATI | 2001 | PCI/AGP | 32/64 MB | DX7 | 2D-possible | radeon UMS: nearly all hw features covered by open drivers. |
| 58 | Radeon 7200 | ATI | 2001 | AGP | 32/64 MB | DX7 | 2D-possible | Same. |
| 59 | Radeon 7500 (RV200) | ATI | 2001 | AGP | 64 MB | DX7 | 2D-possible | Same. |
| 60 | Radeon 8500 (R200) | ATI | 2001 | AGP | 64/128 MB | DX8.1 | 2D-possible | R200 open drivers support nearly all features. |
| 61 | Radeon 9000 (RV250) | ATI | 2002 | AGP | 64/128 MB | DX8.1 | 2D-possible | Same family. |
| 62 | Radeon 9200 (RV280) | ATI | 2003 | AGP | 64/128 MB | DX8.1 | 2D-possible | Same family. |
| 63 | Radeon 9500 (R300) | ATI | 2002 | AGP | 128 MB | DX9 | 2D-possible | r300 open 3D existed; 2D is the bounded target. |
| 64 | Radeon 9700 Pro (R300) | ATI | 2002 | AGP | 128 MB | DX9 | 2D-possible | Same family. |
| 65 | Radeon 9800 Pro (R350) | ATI | 2003 | AGP | 128/256 MB | DX9 | 2D-possible | Same family. |
| 66 | Radeon X300 (RV370) | ATI | 2004 | PCIe | 128 MB | DX9 | 2D-possible | Same family. |
| 67 | Radeon X600 (RV380) | ATI | 2004 | PCIe | 128/256 MB | DX9 | 2D-possible | Same family. |
| 68 | Radeon X800 (R420) | ATI | 2004 | AGP/PCIe | 256 MB | DX9b | 2D-possible | Same family. |
| 69 | Radeon X1300 (RV515) | ATI | 2005 | PCIe | 256 MB | DX9c | 2D-possible | R500; AMD released microcode. |
| 70 | Radeon X1950 XTX (R580) | ATI | 2006 | PCIe | 512 MB | DX9c | 2D-possible | Same. |
| 71 | RIVA 128 | NVIDIA | 1997 | AGP/PCI | 4 MB | DX5 | 2D-possible | nv 2D driver. |
| 72 | RIVA TNT | NVIDIA | 1998 | AGP | 16 MB | DX6 | 2D-possible | Same. |
| 73 | RIVA TNT2 | NVIDIA | 1999 | AGP | 16/32 MB | DX6 | 2D-possible | Same. |
| 74 | GeForce 256 (NV10) | NVIDIA | 1999 | AGP | 32 MB | DX7 | 2D-possible | nouveau covers NV10. |
| 75 | GeForce2 MX (NV11) | NVIDIA | 2000 | AGP | 32/64 MB | DX7 | 2D-possible | Same. |
| 76 | GeForce2 GTS (NV15) | NVIDIA | 2000 | AGP | 32/64 MB | DX7 | 2D-possible | Same. |
| 77 | GeForce3 (NV20) | NVIDIA | 2001 | AGP | 64 MB | DX8 | 2D-possible | Same. |
| 78 | GeForce3 Ti 500 | NVIDIA | 2001 | AGP | 64 MB | DX8 | 2D-possible | Same. |
| 79 | GeForce4 MX 440 (NV17) | NVIDIA | 2002 | AGP | 64 MB | DX7 | 2D-possible | Same. |
| 80 | GeForce4 Ti 4600 (NV25) | NVIDIA | 2002 | AGP | 128 MB | DX8.1 | 2D-possible | Same. |
| 81 | GeForce FX 5200 (NV34) | NVIDIA | 2003 | AGP/PCI | 128 MB | DX9 | 2D-possible | nouveau NV30/NV40 still gets Mesa work (2022). |
| 82 | GeForce FX 5900 (NV35) | NVIDIA | 2003 | AGP | 128/256 MB | DX9 | 2D-possible | Same. |
| 83 | GeForce 6200 (NV44) | NVIDIA | 2004 | PCIe/AGP | 128/256 MB | DX9c | 2D-possible | Same. |
| 84 | GeForce 6600 GT (NV43) | NVIDIA | 2004 | AGP/PCIe | 128 MB | DX9c | 2D-possible | Same. |
| 85 | GeForce 6800 Ultra (NV40) | NVIDIA | 2004 | AGP | 256 MB | DX9c | 2D-possible | Same. |
| 86 | GeForce 7300 GT (G73) | NVIDIA | 2006 | PCIe | 256 MB | DX9c | 2D-possible | Same. |
| 87 | GeForce 7600 GT (G73) | NVIDIA | 2006 | PCIe | 256 MB | DX9c | 2D-possible | Same. |
| 88 | GeForce 7900 GTX (G71) | NVIDIA | 2006 | PCIe | 512 MB | DX9c | 2D-possible | Same. |

3dfx open-sourcing Glide + Voodoo 2/3 specs (Dec 1999):
[Gamespot](https://www.gamespot.com/articles/3dfx-open-sources-glided/1100-2447156/).
tdfx DRI driver:
[XFree86 4.4 DRI doc](http://Xfree86.org/4.4.0/DRI.pdf).
Matrox open 2D specs + XFree86 support:
[mat_linux.pdf](http://afswww.icequake.net/library/pc/mat_linux.pdf).
Intel i810–G965 support list:
[Debian intel(4) manpage](https://manpages.debian.org/stretch/xserver-xorg-video-intel/intel.4.en.html);
[i915 DRM docs](https://docs.kernel.org/6.1/gpu/i915.html).
Radeon R200 open drivers (nearly all features; some specs under NDA, rest
reverse-engineered):
[Wikipedia/Radeon R200](https://en.wikipedia.org/wiki/Radeon_R200_series),
[Phoronix review](https://www.phoronix.com/review/743).
AMD microcode release (R100–R600):
[Phoronix](https://www.phoronix.com/review/amd_microcode).
Nouveau NV30/NV40 still maintained (2022 NIR work):
[Phoronix](https://www.phoronix.com/news/Nouveau-NV30-NV40-NIR);
Nouveau is reverse-engineered:
[Phoronix search](https://www.phoronix.com/search/Nouveau&Submit=Search).

## Tier C — 3D is a full epic (3D-epic)

2D via VESA/open drivers is fine; the 3D engine needs the real-driver
program (command submission + shader compiler).

| # | GPU | Vendor | Year | Bus | VRAM | APIs | Assessment | Rationale |
|---|-----|--------|------|-----|------|------|------------|-----------|
| 89 | Radeon HD 2900 XT (R600) | AMD | 2007 | PCIe | 512 MB | DX10 | 3D-epic | VLIW5 unified shaders; microcode released, but 3D docs came late — full driver program required. |
| 90 | GeForce 8800 GTX (G80) | NVIDIA | 2006 | PCIe | 768 MB | DX10 | 3D-epic | First unified-shader GPU; nouveau covers it, but a GorpOS driver = the epic. |
| 91 | GeForce 8800 GT (G92) | NVIDIA | 2007 | PCIe | 512 MB | DX10 | 3D-epic | Same architecture, era ceiling. |
| 92 | Parhelia | Matrox | 2002 | AGP | 128 MB | DX8.1 | 3D-epic | Docs never opened; triple-head 2D only in practice. |

## Tier D — Forward path (blocked-needs-firmware-or-docs)

Post-2007 hardware included so the roadmap knows where the wall is.
All need signed firmware and/or have zero public docs. NVIDIA's
open-source kernel modules (2022) cover only Turing and later and leave
userspace closed — they do not help the 2007-era cards.

| # | GPU | Vendor | Year | Bus | VRAM | APIs | Assessment | Rationale |
|---|-----|--------|------|-----|------|------|------------|-----------|
| 93 | HD Graphics (Ironlake) | Intel | 2010 | IGP | shared | DX10.1 | blocked-needs-firmware-or-docs | GuC/HuC firmware era begins; no longer firmware-free. |
| 94 | Radeon HD 7970 (GCN) | AMD | 2012 | PCIe | 3 GB | DX11 | blocked-needs-firmware-or-docs | Signed microcode required. |
| 95 | GeForce GTX 680 (Kepler) | NVIDIA | 2012 | PCIe | 2 GB | DX11 | blocked-needs-firmware-or-docs | No docs; firmware signing arrives with Maxwell. |
| 96 | Mali-400 MP | ARM | 2008 | SoC | shared | OGL ES 2.0 | blocked-needs-firmware-or-docs | No open docs at the time. |
| 97 | Adreno 200 | Qualcomm | 2008 | SoC | shared | OGL ES 2.0 | blocked-needs-firmware-or-docs | Same. |
| 98 | PowerVR SGX535 (GMA 500) | Intel/Imagination | 2008 | IGP | shared | DX10.1 | blocked-needs-firmware-or-docs | Infamously closed Poulsbo; binary blob only. |
| 99 | Radeon RX 580 (Polaris) | AMD | 2017 | PCIe | 8 GB | DX12 | blocked-needs-firmware-or-docs | PSP-era signed firmware stack. |
| 100 | Apple M1 GPU | Apple | 2020 | SoC | shared | Metal | blocked-needs-firmware-or-docs | No docs; community reverse-engineered only. |

NVIDIA open modules are Turing+ only:
[TechPowerUp](https://www.techpowerup.com/294781/nvidia-releases-open-source-gpu-kernel-modules?amp).
Maxwell+ signed firmware:
[linux-gaming driver survey](https://linux-gaming.kwindu.eu/index.php?title=Graphic_drivers_on_Linux).

## Analysis

**Distribution:** framebuffer-only: 7 · VESA-ok: 20 · 2D-possible: 56 ·
3D-epic: 4 · blocked-needs-firmware-or-docs: 13 (100 total).

**Decision-relevant findings:**

1. **Our VESA stage already covers ~30% of the table.** Everything in
   Tier A needs no driver at all. The graphics plan should say so
   explicitly: the first 90 GPUs on this list boot under VBE.
2. **The 2D-driver shortlist is: Intel iGPUs, R100–R500 Radeons, Matrox
   G200–G550, and 3dfx Voodoo3.** These four families have open specs or
   working open drivers and no firmware. If Phase 10 ever funds a native
   2D driver, start with **Intel 865G/GMA 950** (most common P4-era
   hardware, fully open) — not with the exotic stuff.
3. **3dfx is the best educational 3D target**, not the best practical one:
   Glide + Voodoo 2/3 specs were open-sourced in Dec 1999 and a tdfx DRI
   driver existed — but the hardware is unobtainable and obsolete. Keep
   it as a reference design, not a milestone.
4. **NVIDIA is a trap for our era:** pre-Maxwell cards need no firmware,
   but there are no docs and Nouveau's P4-era (NV30/NV40) support is
   thin community maintenance. Mark NVIDIA "2D-possible" honestly, but
   plan Intel/AMD-first.
5. **XGI, SiS Xabre, DeltaChrome, and Kyro are documentation write-offs.**
   Don't let nostalgia put them on any milestone: XGI's drivers were bad
   *on Windows* (verified via contemporary reviews), and Kyro's TBDR
   needs a fundamentally different driver architecture.
6. **The forward path hits the firmware wall at ~2008–2010**
   (Ironlake GuC/HuC, AMD signed microcode, Maxwell signing). Any
   post-era port must budget for firmware loading infrastructure in the
   kernel driver model — that belongs in FULL_ROADMAP Phase 10 notes.

## Sources

- Per-GPU specs: TechPowerUp GPU database, Wikipedia family pages
  (individually `[unverified]` unless cited above).
- Cited URLs are listed inline above (Gamespot, XFree86 DRI doc,
  mat_linux.pdf, Debian intel(4), kernel.org i915, Wikipedia R200,
  Phoronix ×3, TechPowerUp ×2, linux-gaming survey, PC Gamer).

*Research log: 6 batches / 17 searches; batches 5–6 confirmed known
facts (Volari V8 2003/DX9, Kyro II 2001, nouveau NV30/NV40 2022 work,
NVIDIA open modules Turing+) rather than novel material; stopped per
the >98%-non-novel rule.*
