# Graphics — scoping for OpenGL / Vulkan on GorpOS

Status labels used in this doc:
- **REAL v1** — we will actually build this on 8086.
- **EMU** — software-emulated on top of v1, no GPU involved.
- **FICTION** — future work, possibly never; labeled honestly.

## Why native OpenGL/Vulkan on 8086 + 10-bit cells is not realistic in v1

1. **No GPU.** OpenGL and Vulkan are GPU APIs. An 8086 machine (1978)
   has no GPU, no shader cores, no texture units. There is nothing to
   drive.
2. **No protected mode / flat memory.** 8086 real mode gives 1 MB via
   segmentation. Mesa (the open-source OpenGL) assumes a 32-bit flat
   address space, a C library, threads, and mmap. None of that exists
   in our Stage 1–5 kernel.
3. **No PCIe / MMIO / DMA as modern GPUs need.** Real GPU drivers
   enumerate PCI, map 64-bit BARs, submit command buffers via DMA, and
   handle MSI-X interrupts. Our kernel has none of: PCI enumeration,
   MMIO mapping, DMA, or an interrupt-driven driver model.
4. **The packed cell model fights pixels.** Our 10/13-bit packed
   regions need shift+mask on every access. A per-pixel hot path doing
   that would be unusably slow. Pixel buffers must live in 8-bit
   regions (see GRAPHICS_COMPAT.md) — which is fine, but it means the
   "10-bit architecture" story does not extend to the framebuffer.
5. **Vulkan's explicit memory model assumes a real allocator.**
   Vulkan wants device memory heaps, dedicated allocations, and
   queue families. Mapping that onto width-tagged packed regions is a
   research project, not a v1 feature.

Conclusion: any OpenGL/Vulkan story on this kernel is software
rendering into a framebuffer for the foreseeable future.

## Stage A — REAL v1: VGA Mode 13h framebuffer console

The actual v1 graphics target. 8086 + VGA gives us:

- Mode 13h: 320x200, 256 colors, linear framebuffer at segment
  `0xA000` (64,000 bytes — fits in one 64K segment, convenient).
- Set via BIOS: `INT 10h, AH=00h, AL=13h`.
- Palette via ports `0x3C8/0x3C9` (DAC index then R/G/B, 6 bits each).

What we build (`src/kernel/fb.h`, `fb.c`):
- `fb_init()` — assumes mode already set by bootloader (or sets it via
  INT 10h on real hardware; host-testable with a malloc'd buffer).
- `fb_clear()`, `fb_put_pixel()`, `fb_get_pixel()`, `fb_blit()`.
- Text console on top: 8x8 font, `fb_put_char()` — the kernel shell
  from Stage 5 renders here instead of (or as well as) text mode.

This is REAL: QEMU's `-vga std` emulates Mode 13h faithfully, so we
can develop and screenshot without any GPU.

## Stage B — EMU: tiny software OpenGL subset

Not Mesa, not full OpenGL — a deliberately small GL 1.x-flavored API
rendered entirely in software into our framebuffer:

- Subset: `glClear`, `glClearColor`, `glBegin/glEnd` (TRIANGLES only),
  `glVertex2f`, `glColor3f`, `glViewport`, `glFlush`. Maybe
  `glOrtho`. That's it for v1.
- Fixed-point or float math on the host side first; 8086 has no FPU
  (8087 optional), so the 8086 rasterizer should use 16.16 fixed point.
- Flat-shaded triangle rasterizer, no depth buffer initially
  (painter's algorithm or a 16-bit z-buffer in an 8-bit region pair).
- Reference: Fabrice Bellard's TinyGL is the right *shape* of project
  (small software GL subset) — we write our own minimal version rather
  than porting, to keep the 8-bit-region constraint and the trit error
  convention.

Where it runs: user space against our kernel, using only the
framebuffer region + timer syscalls from GRAPHICS_COMPAT.md. The GL
subset never touches hardware directly.

Honest limits: software rasterization of even a few hundred triangles
at 320x200 on emulated 8086 will be slow. This is a "it draws a
spinning triangle, isn't that neat" milestone, not a game engine.

## Stage C — FICTION (x86_64 port notes): what a real GPU path needs

If the kernel ever grows an x86_64 long-mode port (see the hardware
notes the main build is producing), graphics would go:

1. Bootloader (UEFI) → GOP linear framebuffer. This is the sane
   "GPU" for a hobby OS: no driver needed, just a framebuffer address.
2. Mesa **llvmpipe** (software OpenGL) as the real OpenGL story —
   still no GPU, but a *complete* OpenGL on CPU. Porting llvmpipe is a
   huge job (LLVM dependency); more realistic is keeping our tiny
   Stage-B subset and growing it.
3. A real GPU driver (even basic modesetting via a simple DRM driver)
   is out of scope indefinitely: needs PCI enumeration, MMIO, DMA,
   interrupt handling, firmware blobs, and a command-stream compiler
   per GPU vendor. We will not get there in this project.

## Vulkan — FICTION, stated bluntly

Real Vulkan needs everything in Stage C plus an explicit driver
model: instance → physical device → logical device → queues →
command buffers → memory heaps. We have none of it, and our packed
memory model is actively hostile to Vulkan's allocation model.

Scoped as **Phase 3, long-term, may never happen**. The most we would
ever do short of a miracle:

- A `vk_min.h` software shim exposing a handful of Vulkan-flavored
  entry points (`vkCreateInstance`, `vkCreateDevice`, ...) that either
  return "not supported" (honest) or route to the Stage-B software
  rasterizer behind the scenes (a lie we would document loudly).
- No WSI (window system integration), no swapchains on 8086.

Do not promise Vulkan to anyone. It is listed here so the roadmap is
honest about where the ceiling is.

## Recommended order

1. Stage A framebuffer + text console (unblocks kernel shell UX).
2. Palette + double-buffer in an 8-bit region (flip via rep movsb).
3. Stage B triangle rasterizer on host, then 8086 fixed-point port.
4. Everything else is future fiction — revisit after x86_64 exists.
