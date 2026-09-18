// alloc.h — dynamic bit-width region allocator (packed)
// Physical backing: uint16_t words. Logical view: width-tagged cells.
// Best-judgment v0: first-fit with coalescing, width is per-region immutable
// (rewidth = alloc new + copy + free). Keeps packed math simple.

#ifndef ALLOC_H
#define ALLOC_H

#include <stdint.h>
#include "cell.h"

#define MAX_REGIONS 64

typedef struct {
    uint8_t  used;      // 0=free slot, 1=allocated
    uint8_t  width;     // 8, 10, 13 (discrete modes)
    uint16_t cells;     // logical cell count
    uint32_t bit_base;  // bit offset into pool
    uint32_t bit_len;   // cells * width
} region_t;

typedef struct {
    uint16_t *pool;          // word pool
    uint32_t  pool_words;
    uint32_t  pool_bits;
    region_t  regions[MAX_REGIONS];
} allocator_t;

void     alloc_init(allocator_t *a, uint16_t *pool, uint32_t pool_words);
int      alloc_cells(allocator_t *a, uint8_t width, uint16_t count); // returns region id or -1
int      alloc_cells_aligned(allocator_t *a, uint8_t width, uint16_t count,
                             uint8_t align_bits); // bit_base % align_bits == 0
void     alloc_free(allocator_t *a, int id);
int      alloc_rewidth(allocator_t *a, int id, uint8_t new_width);   // returns new id or -1
uint16_t region_get(allocator_t *a, int id, uint16_t idx);
void     region_put(allocator_t *a, int id, uint16_t idx, uint16_t v);

#endif
