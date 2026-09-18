// alloc.c — first-fit packed allocator, 8/10/13-bit regions
// Host-testable (no 8086 asm here). Bit_base allocator with simple
// first-fit scan and coalescing on free via region list compaction.

#include "alloc.h"

static int valid_width(uint8_t w) {
    return w == WIDTH_8 || w == WIDTH_10 || w == WIDTH_13;
}

void alloc_init(allocator_t *a, uint16_t *pool, uint32_t pool_words) {
    a->pool = pool;
    a->pool_words = pool_words;
    a->pool_bits = pool_words * 16;
    for (int i = 0; i < MAX_REGIONS; i++) a->regions[i].used = 0;
    for (uint32_t i = 0; i < pool_words; i++) pool[i] = 0;
}

static int find_slot(allocator_t *a) {
    for (int i = 0; i < MAX_REGIONS; i++)
        if (!a->regions[i].used) return i;
    return -1;
}

// crude first-fit: scan bit space for a gap big enough
// If align_bits > 0, candidate bit_base is rounded up to a multiple of it.
static int32_t find_gap_aligned(allocator_t *a, uint32_t need_bits, uint8_t align_bits) {
    uint32_t cursor = 0;
    for (;;) {
        if (align_bits > 1) {
            uint32_t m = (uint32_t)align_bits;
            cursor = (cursor + m - 1) / m * m;
        }
        int overlap = 0;
        for (int i = 0; i < MAX_REGIONS; i++) {
            if (!a->regions[i].used) continue;
            uint32_t s = a->regions[i].bit_base;
            uint32_t e = s + a->regions[i].bit_len;
            if (cursor < e && cursor + need_bits > s) {
                overlap = 1;
                if (e > cursor) cursor = e;
            }
        }
        if (!overlap) {
            if (cursor + need_bits <= a->pool_bits) return (int32_t)cursor;
            return -1;
        }
        if (cursor + need_bits > a->pool_bits) return -1;
    }
}

static int32_t find_gap(allocator_t *a, uint32_t need_bits) {
    return find_gap_aligned(a, need_bits, 0);
}

int alloc_cells(allocator_t *a, uint8_t width, uint16_t count) {
    if (!valid_width(width) || count == 0) return -1;
    int slot = find_slot(a);
    if (slot < 0) return -1;
    uint32_t need = (uint32_t)count * width;
    int32_t at = find_gap(a, need);
    if (at < 0) return -1;
    region_t *r = &a->regions[slot];
    r->used = 1;
    r->width = width;
    r->cells = count;
    r->bit_base = (uint32_t)at;
    r->bit_len = need;
    return slot;
}

int alloc_cells_aligned(allocator_t *a, uint8_t width, uint16_t count,
                        uint8_t align_bits) {
    if (!valid_width(width) || count == 0) return -1;
    if (align_bits > 16) return -1;
    int slot = find_slot(a);
    if (slot < 0) return -1;
    uint32_t need = (uint32_t)count * width;
    int32_t at = find_gap_aligned(a, need, align_bits);
    if (at < 0) return -1;
    region_t *r = &a->regions[slot];
    r->used = 1;
    r->width = width;
    r->cells = count;
    r->bit_base = (uint32_t)at;
    r->bit_len = need;
    return slot;
}

void alloc_free(allocator_t *a, int id) {
    if (id < 0 || id >= MAX_REGIONS) return;
    a->regions[id].used = 0;
}

uint16_t region_get(allocator_t *a, int id, uint16_t idx) {
    region_t *r = &a->regions[id];
    // cell_get_packed expects word-aligned base; we emulate by offsetting
    // into a temp view: compute absolute bit then read via pool
    uint32_t bit = r->bit_base + (uint32_t)idx * r->width;
    uint32_t wi = bit >> 4;
    uint8_t bo = bit & 0xF;
    uint32_t val = a->pool[wi] >> bo;
    if (bo + r->width > 16) val |= (uint32_t)a->pool[wi+1] << (16 - bo);
    uint16_t mask = (1u << r->width) - 1;
    return val & mask;
}

void region_put(allocator_t *a, int id, uint16_t idx, uint16_t v) {
    region_t *r = &a->regions[id];
    uint32_t bit = r->bit_base + (uint32_t)idx * r->width;
    uint32_t wi = bit >> 4;
    uint8_t bo = bit & 0xF;
    uint16_t mask = (1u << r->width) - 1;
    v &= mask;
    uint16_t low_mask = mask << bo;
    a->pool[wi] = (a->pool[wi] & ~low_mask) | ((v << bo) & low_mask);
    if (bo + r->width > 16) {
        uint8_t rem = (bo + r->width) - 16;
        uint16_t hm = (1u << rem) - 1;
        a->pool[wi+1] = (a->pool[wi+1] & ~hm) | ((v >> (16 - bo)) & hm);
    }
}

int alloc_rewidth(allocator_t *a, int id, uint8_t new_width) {
    if (id < 0 || id >= MAX_REGIONS || !a->regions[id].used) return -1;
    if (!valid_width(new_width)) return -1;
    region_t old = a->regions[id];
    if (old.width == new_width) return id;
    int nid = alloc_cells(a, new_width, old.cells);
    if (nid < 0) return -1;
    uint16_t minw = old.width < new_width ? old.width : new_width;
    uint16_t m = (1u << minw) - 1;
    for (uint16_t i = 0; i < old.cells; i++) {
        uint16_t v = region_get(a, id, i) & m;
        region_put(a, nid, i, v);
    }
    alloc_free(a, id);
    return nid;
}
