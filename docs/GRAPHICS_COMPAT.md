# Graphics compatibility layer — kernel plan

How the alien parts of this kernel (packed 8/10/13-bit allocator,
ternary returns, DOS INT 10h shim) serve a graphics stack without
pretending to be something they're not.

## 1. Syscalls / ABIs the graphics layer needs from the kernel

Minimal set — everything graphics does goes through these; the GL
subset and console never touch hardware directly:

| # | Call | Semantics |
|---|------|-----------|
| G1 | `k_fb_acquire(w, h)` | Returns a framebuffer handle + 8-bit region id. T_TRUE = granted, T_FALSE = unsupported mode, T_UNKNOWN = try again later (no free region). |
| G2 | `k_fb_present(handle)` | Flip: copy back-buffer region to the visible VGA window (`rep movsb` on 8086). T_TRUE = flipped. |
| G3 | `k_timer_hz(n)` | Vsync-ish tick: program the PIT (port 0x40–0x43) for n Hz; delivers a tick event the rasterizer can poll. On 8086 there is no vsync interrupt wired to us in v1 — the PIT tick is the honest substitute. |
| G4 | `k_input_poll(&ev)` | Keyboard event (make/break scancode via INT 9h handler ring buffer, or BIOS INT 16h polling in v0). T_UNKNOWN = no event waiting. |
| G5 | `k_palette_set(idx, r, g, b)` | Program VGA DAC (ports 0x3C8/0x3C9). Needed because Mode 13h is palette-indexed. |

Deliberately absent in v1: `mmap`, DMA-BUF, ioctls, GPU command
submission, vsync IRQ. Those belong to the x86_64 fiction phase.

## 2. How the packed allocator serves pixel buffers

**Rule: pixel data always lives in 8-bit regions. No exceptions in v1.**

- A 320x200 framebuffer is exactly 64,000 bytes = 64,000 8-bit cells.
  In an 8-bit packed region that is bit-identical to a flat byte
  array, so `fb->pixels[i]` is a plain indexed store — no shift/mask
  in the hot path.
- 10-bit and 13-bit regions are for *metadata*: vertex lists,
  palettes (10-bit gives 1024 DAC steps headroom), fixed-point
  triangle edge data. They never sit under a per-pixel loop.
- Double buffering: two 8-bit regions (front/back), `k_fb_present`
  copies back→front. 128 KB total — fine within the 1 MB real-mode
  budget if placed carefully (conventional memory is 640 KB; the
  allocator must be told the VGA window 0xA0000–0xAFFFF is reserved).

Why not pack pixels into 10-bit cells? A 10-bit pixel *sounds* fun
until you measure: every pixel read becomes shift+mask+possible
cross-word merge. Fill rate would collapse, and VGA DAC indices are
bytes anyway. The 8-bit rule is a deliberate, documented compromise:
the alien architecture lives in kernel metadata, the pixels stay
boring and fast.

## 3. Ternary returns → GL-style error codes

OpenGL has no "unknown". Mapping at the compat boundary:

- `T_TRUE` → `GL_NO_ERROR` / success. Nothing to report.
- `T_FALSE` → a real GL error: `GL_INVALID_VALUE` (bad coords/mode),
  `GL_INVALID_OPERATION` (bad state), `GL_OUT_OF_MEMORY` (alloc fail).
  The shim chooses the closest enum and sets it via `glGetError`
  semantics.
- `T_UNKNOWN` → **retry inside the kernel boundary, never leak it.**
  The compat layer spins or defers (scheduler understands T_UNKNOWN
  as defer/retry per Stage 3), and only returns to GL-land once it
  resolves to TRUE/FALSE. If it cannot resolve, it degrades to
  `GL_OUT_OF_MEMORY` — the closest honest GL error — and logs the
  ternary stall in the kernel log.

Rationale: ternary logic is a kernel-internal convention. Foreign APIs
(GL, DOS, V4) get strict binary answers; the interesting third value
never crosses the ABI.

## 4. DOS compat: INT 10h video services through our fb layer

DOS programs expect BIOS video services. Our INT 10h shim translates
the useful subset into `fb_*` calls (Phase 1 scope):

- `AH=00h` set mode → only `AL=13h` (and `03h` text) honored;
  others return T_FALSE (mapped to "unsupported" — we do not fake
  modes we can't render).
- `AH=0Ch` write pixel → `fb_put_pixel`; `AH=0Dh` read pixel →
  `fb_get_pixel`. Note: on real 8086 hardware a DOS program could
  also just write to `0xA000` directly and bypass us — the shim does
  not (and cannot) prevent that; it only serves programs that go
  through the BIOS.
- `AH=13h` write string → our text-console renderer on top of fb.
- Palette (`AH=10h` subfunctions) → `k_palette_set`.

Honest boundary: we emulate the *services*, not the VGA hardware.
QEMU's emulated VGA does the hardware part.

## 5. What the kernel must NOT do for graphics in v1

- No GPU command stream, no shader compiler, no modesetting beyond
  INT 10h.
- No taking over INT 10h from programs that bang hardware directly.
- No Vulkan loader, no WSI. (See GRAPHICS.md — Vulkan is Phase 3
  fiction.)
- No 10-bit-packed pixel buffers in any hot path (see §2).

## 6. Test plan (host-first, per project convention)

1. `fb_*` unit tests with `-DFB_HOST` (init/clear/pixel/blit/clip).
2. Software rasterizer tests on host writing into an `fb_t`, dump to
   PPM for visual inspection.
3. QEMU: boot to Mode 13h, clear to a color, draw test pattern —
   screenshot via QEMU monitor to confirm.
4. DOS shim: a `.COM` that calls INT 10h AH=0Ch and INT 21h AH=09h,
   verify pixel + text appear.
