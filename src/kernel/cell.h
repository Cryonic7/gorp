// cell.h — packed variable-width cells on 8086
// Supports discrete 8, 10, 13-bit modes, packed bit-stream across 16-bit words.
// No per-cell padding in v1; every access is shift+mask.

#ifndef CELL_H
#define CELL_H

#include <stdint.h>

#define WIDTH_8  8
#define WIDTH_10 10
#define WIDTH_13 13

// bit offset of cell idx in a packed stream of given width
static inline uint32_t cell_bit_offset(uint16_t idx, uint8_t width) {
    return (uint32_t)idx * width;
}

// Get a cell (width 8/10/13) from packed uint16_t array.
// Widths <16 so a cell spans at most 2 words.
static inline uint16_t cell_get_packed(const uint16_t *base, uint16_t idx, uint8_t width) {
    uint32_t bit = cell_bit_offset(idx, width);
    uint32_t wi = bit >> 4;        // /16
    uint8_t  bo = bit & 0xF;       // %16
    uint32_t val = base[wi] >> bo;
    if (bo + width > 16) {
        val |= (uint32_t)base[wi + 1] << (16 - bo);
    }
    uint16_t mask = (width == 16) ? 0xFFFF : ((1u << width) - 1);
    return (uint16_t)(val & mask);
}

static inline void cell_put_packed(uint16_t *base, uint16_t idx, uint8_t width, uint16_t v) {
    uint32_t bit = cell_bit_offset(idx, width);
    uint32_t wi = bit >> 4;
    uint8_t  bo = bit & 0xF;
    uint16_t mask = (width == 16) ? 0xFFFF : ((1u << width) - 1);
    v &= mask;

    // clear + set low part
    uint16_t low_mask = (uint16_t)(mask << bo);
    base[wi] = (base[wi] & ~low_mask) | ((v << bo) & low_mask);

    if (bo + width > 16) {
        uint8_t rem = (bo + width) - 16; // bits spilling into next word
        uint16_t high_mask = (1u << rem) - 1;
        base[wi + 1] = (base[wi + 1] & ~high_mask) | ((v >> (16 - bo)) & high_mask);
    }
}

// words needed to hold `count` cells of `width` bits
static inline uint16_t cells_words_needed(uint16_t count, uint8_t width) {
    uint32_t bits = (uint32_t)count * width;
    return (bits + 15) >> 4;
}

#endif
