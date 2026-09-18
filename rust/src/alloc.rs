//! Dynamic bit-width region allocator (packed).
//! Mirror of `src/kernel/alloc.h` / `alloc.c`.
//!
//! Physical backing: a `&mut [u16]` pool. Logical view: width-tagged
//! cell regions. First-fit over bit space; `rewidth` = alloc new +
//! masked copy + free (not atomic, old region intact on failure).

use crate::cell::{WIDTH_10, WIDTH_13, WIDTH_8};

pub const MAX_REGIONS: usize = 64;

#[derive(Debug, Clone, Copy)]
struct Region {
    used: bool,
    width: u8,
    cells: u16,
    bit_base: u32,
    bit_len: u32,
}

impl Region {
    const fn empty() -> Region {
        Region {
            used: false,
            width: 0,
            cells: 0,
            bit_base: 0,
            bit_len: 0,
        }
    }
}

fn valid_width(w: u8) -> bool {
    w == WIDTH_8 || w == WIDTH_10 || w == WIDTH_13
}

pub struct Allocator<'a> {
    pool: &'a mut [u16],
    pool_bits: u32,
    regions: [Region; MAX_REGIONS],
}

impl<'a> Allocator<'a> {
    pub fn new(pool: &'a mut [u16]) -> Allocator<'a> {
        let bits = pool.len() as u32 * 16;
        for w in pool.iter_mut() {
            *w = 0;
        }
        Allocator {
            pool,
            pool_bits: bits,
            regions: [Region::empty(); MAX_REGIONS],
        }
    }

    fn find_slot(&self) -> Option<usize> {
        self.regions.iter().position(|r| !r.used)
    }

    /// First-fit scan of bit space; `align_bits` rounds candidates up.
    fn find_gap(&self, need_bits: u32, align_bits: u8) -> Option<u32> {
        let mut cursor: u32 = 0;
        loop {
            if align_bits > 1 {
                let m = align_bits as u32;
                cursor = cursor.div_ceil(m) * m;
            }
            let mut overlap = false;
            for r in self.regions.iter().filter(|r| r.used) {
                let s = r.bit_base;
                let e = s + r.bit_len;
                if cursor < e && cursor + need_bits > s {
                    overlap = true;
                    if e > cursor {
                        cursor = e;
                    }
                }
            }
            if !overlap {
                return if cursor + need_bits <= self.pool_bits {
                    Some(cursor)
                } else {
                    None
                };
            }
            if cursor + need_bits > self.pool_bits {
                return None;
            }
        }
    }

    fn alloc_inner(&mut self, width: u8, count: u16, align_bits: u8) -> Option<usize> {
        if !valid_width(width) || count == 0 || align_bits > 16 {
            return None;
        }
        let slot = self.find_slot()?;
        let need = count as u32 * width as u32;
        let at = self.find_gap(need, align_bits)?;
        self.regions[slot] = Region {
            used: true,
            width,
            cells: count,
            bit_base: at,
            bit_len: need,
        };
        Some(slot)
    }

    /// Allocate `count` cells of `width` bits. Returns region id.
    pub fn alloc_cells(&mut self, width: u8, count: u16) -> Option<usize> {
        self.alloc_inner(width, count, 0)
    }

    /// Like `alloc_cells`, but `bit_base` is a multiple of `align_bits`.
    pub fn alloc_cells_aligned(&mut self, width: u8, count: u16, align_bits: u8) -> Option<usize> {
        self.alloc_inner(width, count, align_bits)
    }

    pub fn free(&mut self, id: usize) {
        if id < MAX_REGIONS {
            self.regions[id].used = false;
        }
    }

    fn region(&self, id: usize) -> Option<&Region> {
        self.regions.get(id).filter(|r| r.used)
    }

    /// Absolute bit offset of cell `idx` in region `id`.
    fn abs_bit(&self, id: usize, idx: u16) -> Option<(u32, u8)> {
        let r = self.region(id)?;
        if idx >= r.cells {
            return None;
        }
        let bit = r.bit_base + idx as u32 * r.width as u32;
        Some((bit, r.width))
    }

    pub fn get(&self, id: usize, idx: u16) -> Option<u16> {
        let (bit, width) = self.abs_bit(id, idx)?;
        let wi = (bit >> 4) as usize;
        let bo = (bit & 0xF) as u8;
        let mut val = (self.pool[wi] >> bo) as u32;
        if bo + width > 16 {
            val |= (self.pool[wi + 1] as u32) << (16 - bo);
        }
        let mask = (1u32 << width) - 1;
        Some((val & mask) as u16)
    }

    pub fn put(&mut self, id: usize, idx: u16, v: u16) -> bool {
        let (bit, width) = match self.abs_bit(id, idx) {
            Some(x) => x,
            None => return false,
        };
        let wi = (bit >> 4) as usize;
        let bo = (bit & 0xF) as u8;
        let mask = ((1u32 << width) - 1) as u16;
        let v = v & mask;
        let low_mask = (((mask as u32) << bo) & 0xFFFF) as u16;
        self.pool[wi] = (self.pool[wi] & !low_mask) | ((((v as u32) << bo) as u16) & low_mask);
        if bo + width > 16 {
            let rem = bo + width - 16;
            let high_mask = ((1u32 << rem) - 1) as u16;
            self.pool[wi + 1] = (self.pool[wi + 1] & !high_mask) | ((v >> (16 - bo)) & high_mask);
        }
        true
    }

    /// Change a region's width (copy with min-width mask). Returns new id;
    /// old region is freed. On failure the old region is untouched.
    pub fn rewidth(&mut self, id: usize, new_width: u8) -> Option<usize> {
        let old = *self.region(id)?;
        if !valid_width(new_width) {
            return None;
        }
        if old.width == new_width {
            return Some(id);
        }
        let nid = self.alloc_cells(new_width, old.cells)?;
        let minw = old.width.min(new_width);
        let m = ((1u32 << minw) - 1) as u16;
        for i in 0..old.cells {
            let v = self.get(id, i).unwrap_or(0) & m;
            self.put(nid, i, v);
        }
        self.free(id);
        Some(nid)
    }

    /// Test helper: raw region metadata.
    #[cfg(test)]
    fn region_info(&self, id: usize) -> Option<(u8, u16, u32)> {
        self.region(id).map(|r| (r.width, r.cells, r.bit_base))
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn alloc_and_roundtrip() {
        let mut pool = [0u16; 256];
        let mut a = Allocator::new(&mut pool);
        let r8 = a.alloc_cells(WIDTH_8, 4).unwrap();
        let r10 = a.alloc_cells(WIDTH_10, 4).unwrap();
        let r13 = a.alloc_cells(WIDTH_13, 4).unwrap();
        a.put(r8, 0, 0xAB);
        a.put(r10, 0, 0x3FF);
        a.put(r13, 0, 0x1FFF);
        assert_eq!(a.get(r8, 0), Some(0xAB));
        assert_eq!(a.get(r10, 0), Some(0x3FF));
        assert_eq!(a.get(r13, 0), Some(0x1FFF));
    }

    #[test]
    fn aligned_placement() {
        let mut pool = [0u16; 256];
        let mut a = Allocator::new(&mut pool);
        // fragment, then require byte alignment
        let _ = a.alloc_cells(WIDTH_13, 3).unwrap();
        let id = a.alloc_cells_aligned(WIDTH_8, 16, 8).unwrap();
        let (_, _, base) = a.region_info(id).unwrap();
        assert_eq!(base % 8, 0);
    }

    #[test]
    fn rewidth_truncates_and_extends() {
        let mut pool = [0u16; 256];
        let mut a = Allocator::new(&mut pool);
        let r = a.alloc_cells(WIDTH_10, 2).unwrap();
        a.put(r, 0, 0x3FF);
        let r2 = a.rewidth(r, WIDTH_8).unwrap();
        assert_eq!(a.get(r2, 0), Some(0xFF)); // low 8 bits kept
        let r3 = a.rewidth(r2, WIDTH_13).unwrap();
        assert_eq!(a.get(r3, 0), Some(0xFF)); // zero-extended
    }

    #[test]
    fn oom_and_bad_width() {
        let mut pool = [0u16; 4]; // 64 bits
        let mut a = Allocator::new(&mut pool);
        assert!(a.alloc_cells(7, 4).is_none()); // invalid width
        assert!(a.alloc_cells(WIDTH_8, 0).is_none()); // zero count
        assert!(a.alloc_cells(WIDTH_13, 64).is_none()); // too big
        assert!(a.alloc_cells(WIDTH_8, 8).is_some()); // exactly 64 bits
    }

    #[test]
    fn free_reuses_space() {
        let mut pool = [0u16; 16];
        let mut a = Allocator::new(&mut pool);
        let r = a.alloc_cells(WIDTH_10, 8).unwrap();
        a.free(r);
        assert!(a.alloc_cells(WIDTH_10, 8).is_some());
    }
}
