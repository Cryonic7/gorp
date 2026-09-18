//! Packed variable-width cells on 16-bit words.
//! Mirror of `src/kernel/cell.h` (packed v1).
//!
//! Semantics match the C exactly, including the deliberate u16
//! truncation in the low-word mask of `cell_put_packed`.

pub const WIDTH_8: u8 = 8;
pub const WIDTH_10: u8 = 10;
pub const WIDTH_13: u8 = 13;

/// Bit offset of cell `idx` in a packed stream of the given width.
#[inline]
pub fn cell_bit_offset(idx: u16, width: u8) -> u32 {
    idx as u32 * width as u32
}

fn width_mask(width: u8) -> u16 {
    if width == 16 {
        0xFFFF
    } else {
        ((1u32 << width) - 1) as u16
    }
}

/// Get a cell (width <= 16) from a packed u16 slice.
/// A cell spans at most 2 words.
pub fn cell_get_packed(base: &[u16], idx: u16, width: u8) -> u16 {
    let bit = cell_bit_offset(idx, width);
    let wi = (bit >> 4) as usize;
    let bo = (bit & 0xF) as u8;
    let mut val = (base[wi] >> bo) as u32;
    if bo + width > 16 {
        val |= (base[wi + 1] as u32) << (16 - bo);
    }
    (val & width_mask(width) as u32) as u16
}

/// Put a cell (width <= 16) into a packed u16 slice.
pub fn cell_put_packed(base: &mut [u16], idx: u16, width: u8, v: u16) {
    let bit = cell_bit_offset(idx, width);
    let wi = (bit >> 4) as usize;
    let bo = (bit & 0xF) as u8;
    let mask = width_mask(width);
    let v = v & mask;

    // low word: clear then set bits [bo, 16). The mask is truncated to
    // 16 bits exactly like the C `(uint16_t)(mask << bo)`.
    let low_mask = (((mask as u32) << bo) & 0xFFFF) as u16;
    base[wi] = (base[wi] & !low_mask) | (((v as u32) << bo) as u16 & low_mask);

    if bo + width > 16 {
        let rem = bo + width - 16; // bits spilling into next word
        let high_mask = ((1u32 << rem) - 1) as u16;
        base[wi + 1] = (base[wi + 1] & !high_mask) | ((v >> (16 - bo)) & high_mask);
    }
}

/// Words needed to hold `count` cells of `width` bits.
#[inline]
pub fn cells_words_needed(count: u16, width: u8) -> u16 {
    let bits = count as u32 * width as u32;
    ((bits + 15) >> 4) as u16
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn roundtrip_all_widths() {
        for &w in &[WIDTH_8, WIDTH_10, WIDTH_13] {
            let mut buf = [0u16; 32];
            let vals: [u16; 8] = [0xAB, 0x3FF, 0x155, 0x1FFF, 0x0ABC, 0x001, 0x1FFF, 0x2A5];
            for (i, &v) in vals.iter().enumerate() {
                cell_put_packed(&mut buf, i as u16, w, v);
            }
            let mask = width_mask(w);
            for (i, &v) in vals.iter().enumerate() {
                assert_eq!(cell_get_packed(&buf, i as u16, w), v & mask, "w={} i={}", w, i);
            }
        }
    }

    #[test]
    fn spill_over_word_boundary() {
        // 13-bit cells: cell 1 starts at bit 13, spills into word 1.
        let mut buf = [0u16; 4];
        cell_put_packed(&mut buf, 0, 13, 0x1FFF);
        cell_put_packed(&mut buf, 1, 13, 0x0ABC);
        assert_eq!(cell_get_packed(&buf, 0, 13), 0x1FFF);
        assert_eq!(cell_get_packed(&buf, 1, 13), 0x0ABC);
        // cell 0 must not corrupt cell 1 and vice versa
        cell_put_packed(&mut buf, 0, 13, 0x0000);
        assert_eq!(cell_get_packed(&buf, 1, 13), 0x0ABC);
    }

    #[test]
    fn words_needed_math() {
        assert_eq!(cells_words_needed(4, 8), 2);
        assert_eq!(cells_words_needed(8, 10), 5); // 80 bits -> 5 words
        assert_eq!(cells_words_needed(4, 13), 4); // 52 bits -> 4 words
    }
}
