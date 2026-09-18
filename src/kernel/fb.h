// fb.h — framebuffer abstraction, VGA Mode 13h (320x200, 256 colors)
// REAL v1 graphics target. Pixel data lives in 8-bit regions: one byte
// per pixel = one VGA DAC palette index. No packing in the hot path.
//
// On 8086 real mode the framebuffer is the VGA window at segment 0xA000.
// For host testing, compile with -DFB_HOST to use a malloc'd buffer.

#ifndef GORPOS_FB_H
#define GORPOS_FB_H

#include <stdint.h>
#include "trit.h"

#define FB_W      320
#define FB_H      200
#define FB_PIXELS ((uint32_t)FB_W * FB_H) // 64000
#define FB_MODE13 0x13

typedef struct {
    uint8_t *pixels; // FB_PIXELS bytes, 8-bit color indices
    uint16_t w;
    uint16_t h;
    uint8_t  mode;   // BIOS video mode in effect
    uint8_t  owned;  // 1 if we malloc'd pixels (host), 0 if HW-mapped
} fb_t;

// T_TRUE=ok, T_FALSE=failed (bad mode / no memory)
trit_t fb_init(fb_t *fb);
void   fb_shutdown(fb_t *fb);

// T_TRUE=ok, T_FALSE=out of bounds
trit_t  fb_clear(fb_t *fb, uint8_t color);
trit_t  fb_put_pixel(fb_t *fb, uint16_t x, uint16_t y, uint8_t color);
uint8_t fb_get_pixel(const fb_t *fb, uint16_t x, uint16_t y); // 0 if OOB

// Copy w*h bytes from src into fb at (x,y), clipped.
// T_TRUE=ok (possibly clipped), T_FALSE=entirely off-screen
trit_t fb_blit(fb_t *fb, uint16_t x, uint16_t y,
               uint16_t w, uint16_t h, const uint8_t *src);

// 8086 only: set VGA Mode 13h via BIOS INT 10h. Host: no-op, T_UNKNOWN.
trit_t fb_set_mode13(void);

#endif // GORPOS_FB_H
