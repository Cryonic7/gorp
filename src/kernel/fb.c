// fb.c — VGA Mode 13h framebuffer, host-testable stubs
//
// 8086 real mode: pixels map to segment 0xA000:0000 (64K window, we use 64000).
// Mode set is the bootloader/BIOS's job (INT 10h AH=00 AL=13h); fb_init
// just binds to the window. Palette programming (ports 0x3C8/0x3C9) is
// left to a later palette module.
//
// Host (FB_HOST): malloc a 64000-byte buffer so the rasterizer and
// blit/clip logic can be unit-tested without hardware.

#include "fb.h"

#ifdef FB_HOST
#include <stdlib.h>
#include <string.h>
#define FB_SEG_PTR ((uint8_t*)0) // unused on host
#else
// 8086 real-mode far pointer to VGA window. Built with a 16-bit toolchain
// (e.g. ia16-gcc); on other targets this file is not compiled for HW.
#define FB_SEG_PTR ((uint8_t*)0) // placeholder: wire to 0xA000:0000 in hw port
#endif

static int in_bounds(uint16_t x, uint16_t y) {
    return x < FB_W && y < FB_H;
}

trit_t fb_init(fb_t *fb) {
    if (!fb) return T_FALSE;
#ifdef FB_HOST
    fb->pixels = (uint8_t *)malloc(FB_PIXELS);
    if (!fb->pixels) return T_FALSE;
    fb->owned = 1;
#else
    // Real HW: bind to VGA window. The hw port replaces this with a far
    // pointer to 0xA000:0000 and sets owned=0. We cannot dereference it
    // meaningfully in a flat host build, so this is intentionally a stub
    // until the 16-bit toolchain port lands.
    fb->pixels = FB_SEG_PTR;
    fb->owned = 0;
    if (!fb->pixels) return T_FALSE; // stub guard: no HW mapping yet
#endif
    fb->w = FB_W;
    fb->h = FB_H;
    fb->mode = FB_MODE13;
    return T_TRUE;
}

void fb_shutdown(fb_t *fb) {
    if (!fb) return;
#ifdef FB_HOST
    if (fb->owned) free(fb->pixels);
#endif
    fb->pixels = 0;
    fb->owned = 0;
}

trit_t fb_clear(fb_t *fb, uint8_t color) {
    if (!fb || !fb->pixels) return T_FALSE;
#ifdef FB_HOST
    memset(fb->pixels, color, FB_PIXELS);
#else
    for (uint32_t i = 0; i < FB_PIXELS; i++) fb->pixels[i] = color;
#endif
    return T_TRUE;
}

trit_t fb_put_pixel(fb_t *fb, uint16_t x, uint16_t y, uint8_t color) {
    if (!fb || !fb->pixels) return T_FALSE;
    if (!in_bounds(x, y)) return T_FALSE;
    fb->pixels[(uint32_t)y * FB_W + x] = color;
    return T_TRUE;
}

uint8_t fb_get_pixel(const fb_t *fb, uint16_t x, uint16_t y) {
    if (!fb || !fb->pixels) return 0;
    if (!in_bounds(x, y)) return 0;
    return fb->pixels[(uint32_t)y * FB_W + x];
}

trit_t fb_blit(fb_t *fb, uint16_t x, uint16_t y,
               uint16_t w, uint16_t h, const uint8_t *src) {
    if (!fb || !fb->pixels || !src) return T_FALSE;
    if (x >= FB_W || y >= FB_H) return T_FALSE; // entirely off-screen

    // clip to screen
    uint16_t cw = w, ch = h;
    if ((uint32_t)x + cw > FB_W) cw = FB_W - x;
    if ((uint32_t)y + ch > FB_H) ch = FB_H - y;

    for (uint16_t row = 0; row < ch; row++) {
        uint8_t *dst = &fb->pixels[(uint32_t)(y + row) * FB_W + x];
        const uint8_t *s = &src[(uint32_t)row * w];
        for (uint16_t col = 0; col < cw; col++) dst[col] = s[col];
    }
    return T_TRUE;
}

trit_t fb_set_mode13(void) {
#ifdef FB_HOST
    return T_UNKNOWN; // no BIOS on host; caller should treat as "not applicable"
#else
    // 8086: INT 10h, AH=00h, AL=13h. Implemented in the hw port's asm;
    // this C stub exists so the call graph compiles.
    // asm volatile ("int $0x10" : : "a"(0x0013) : "memory");
    return T_UNKNOWN; // unimplemented until 16-bit toolchain port
#endif
}
