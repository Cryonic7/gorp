// dos_compat.c — DOS .COM / MZ .EXE loader + INT 21h shim
//
// HOST-TESTABLE: all logic here is pure C on the allocator. The only
// hardware touchpoint is s->putchar_fn, which on real 8086 would be a
// BIOS INT 10h teletype call; on host it defaults to putchar().
//
// What this is NOT:
//  - Not a DOS kernel: no PSP beyond the load address, no FCB support,
//    no device drivers, no TSR handling.
//  - MZ checksum, overlay number, and min_extra/max_extra paragraphs are
//    parsed but not enforced (documented limitation).
//  - Relocations assume a flat load (load segment paragraph = 0). The
//    addend is still applied so the algorithm is correct if that changes.

#include "dos_compat.h"
#include <stdio.h>   // host putchar default; replace on target
#include <string.h>

#define REG_AX 0
#define REG_BX 1
#define REG_CX 2
#define REG_DX 3
#define REG_SI 4
#define REG_DI 5
#define REG_DS 6
#define REG_ES 7

static void default_putchar(char c) { putchar(c); }

bin_type_t dos_detect(const uint8_t *img, uint32_t len) {
    if (!img || len < 2) return BIN_RAW_COM;
    if (img[0] == 'M' && img[1] == 'Z') return BIN_MZ_EXE;
    if (img[0] == 'N' && img[1] == 'E') return BIN_NE_WIN31;
    return BIN_RAW_COM; // DOS rule: anything not MZ is treated as .COM
}

static uint16_t rd16le(const uint8_t *p) {
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

// Copy `n` bytes into an 8-bit region at cell offset `dst_cell`.
static void region_write_bytes(dos_session_t *s, uint32_t dst_cell,
                               const uint8_t *src, uint32_t n) {
    for (uint32_t i = 0; i < n; i++)
        region_put(s->a, s->region_id, (uint16_t)(dst_cell + i), src[i]);
}

static uint8_t region_read_byte(dos_session_t *s, uint32_t cell) {
    return (uint8_t)region_get(s->a, s->region_id, (uint16_t)cell);
}

// --- .COM loader -------------------------------------------------------
static trit_t load_com(allocator_t *a, const uint8_t *img, uint32_t len,
                       dos_session_t *out) {
    // .COM: single 64K segment, image at 0x100.
    // cells must fit in uint16_t (alloc_cells takes uint16_t count).
    uint32_t cells = DOS_COM_LOAD_ADDR + len;
    if (cells > 0xFFFFu) return T_FALSE;
    int id = alloc_cells_aligned(a, WIDTH_8, (uint16_t)cells, 8);
    if (id < 0) return T_UNKNOWN;

    out->a = a;
    out->region_id = id;
    out->type = BIN_RAW_COM;
    out->cs = 0;
    out->ip = DOS_COM_LOAD_ADDR;
    out->terminated = 0;

    region_write_bytes(out, DOS_COM_LOAD_ADDR, img, len);
    return T_TRUE;
}

// --- MZ .EXE loader ----------------------------------------------------
static trit_t load_mz(allocator_t *a, const uint8_t *img, uint32_t len,
                      dos_session_t *out) {
    if (len < sizeof(mz_header_t)) return T_FALSE;

    mz_header_t h;
    memcpy(&h, img, sizeof(h)); // avoid unaligned access

    if (h.sig != 0x5A4D) return T_FALSE;

    uint32_t hdr_size = (uint32_t)h.hdr_paras * 16u;
    if (hdr_size > len) return T_FALSE;
    if (h.hdr_paras == 0) return T_FALSE;

    // Relocation table must lie inside the file.
    uint32_t reloc_end = (uint32_t)h.reloc_tab + (uint32_t)h.relocs * 4u;
    if (reloc_end > len) return T_FALSE;

    // File image size from blocks/last_bytes (last_bytes==0 => full 512).
    if (h.blocks == 0) return T_FALSE;
    uint32_t fsize = (uint32_t)h.blocks * 512u;
    if (h.last_bytes) {
        if (h.last_bytes > 512) return T_FALSE;
        fsize -= (512u - h.last_bytes);
    }
    if (fsize > len) fsize = len; // tolerate short reads of trailing bytes
    if (fsize < hdr_size) return T_FALSE;

    uint32_t data_len = fsize - hdr_size;
    if (data_len > 0xFFFFu) return T_FALSE; // keep cell counts in range

    int id = alloc_cells_aligned(a, WIDTH_8, (uint16_t)data_len, 8);
    if (id < 0) return T_UNKNOWN;

    out->a = a;
    out->region_id = id;
    out->type = BIN_MZ_EXE;
    out->cs = h.cs;
    out->ip = h.ip;
    out->terminated = 0;

    // Copy load module (header stripped).
    region_write_bytes(out, 0, img + hdr_size, data_len);

    // Apply relocations: each entry is (offset, segment); the word at
    // file offset hdr_size + seg*16 + off gets load_para added.
    // Flat model: load_para = 0, but apply the addend for correctness.
    const uint16_t load_para = 0;
    for (uint16_t i = 0; i < h.relocs; i++) {
        const uint8_t *e = img + h.reloc_tab + (uint32_t)i * 4u;
        uint16_t roff = rd16le(e);
        uint16_t rseg = rd16le(e + 2);
        uint32_t fix_file_off = hdr_size + (uint32_t)rseg * 16u + roff;
        if (fix_file_off + 2 > fsize) {
            // Corrupt table: don't leak the half-loaded region.
            alloc_free(a, id);
            out->region_id = -1;
            return T_FALSE;
        }
        uint32_t cell = fix_file_off - hdr_size;
        uint16_t w = (uint16_t)region_read_byte(out, cell)
                   | ((uint16_t)region_read_byte(out, cell + 1) << 8);
        w += load_para;
        region_put(out->a, out->region_id, (uint16_t)cell, (uint8_t)(w & 0xFF));
        region_put(out->a, out->region_id, (uint16_t)(cell + 1), (uint8_t)(w >> 8));
    }
    return T_TRUE;
}

trit_t dos_load(allocator_t *a, const uint8_t *img, uint32_t len,
                dos_session_t *out) {
    if (!a || !img || !out) return T_FALSE;
    void (*hook)(char) = out->putchar_fn; // preserved across init
    memset(out, 0, sizeof(*out));
    out->putchar_fn = hook ? hook : default_putchar;
    switch (dos_detect(img, len)) {
        case BIN_MZ_EXE: return load_mz(a, img, len, out);
        case BIN_NE_WIN31: return T_FALSE; // Phase 2, see win31_load_ne
        default: return load_com(a, img, len, out);
    }
}

void dos_unload(dos_session_t *s) {
    if (!s || !s->a || s->region_id < 0) return;
    alloc_free(s->a, s->region_id);
    s->region_id = -1;
}

// --- INT 21h shim ------------------------------------------------------
// Linear address of DS:DX in our flat model == cell index (region is
// byte-aligned 8-bit, loaded at linear 0).
static int32_t dos_linear(dos_session_t *s, uint16_t seg, uint16_t off,
                          uint32_t *cells_out) {
    uint32_t lin = (uint32_t)seg * 16u + off;
    lin &= 0xFFFFFu; // 20-bit wraparound, like real 8086
    uint32_t ncells = s->a->regions[s->region_id].cells;
    if (lin >= ncells) return -1;
    *cells_out = lin;
    return 0;
}

static trit_t int21_09(dos_session_t *s, uint16_t *regs) {
    // AH=09h: print $-terminated string at DS:DX
    uint32_t cell;
    if (dos_linear(s, regs[REG_DS], regs[REG_DX], &cell) != 0) return T_FALSE;
    uint32_t ncells = s->a->regions[s->region_id].cells;
    for (uint32_t i = 0; i < 4096 && cell + i < ncells; i++) {
        char c = (char)region_read_byte(s, cell + i);
        if (c == '$') return T_TRUE;
        s->putchar_fn(c);
    }
    return T_FALSE; // no terminator found: malformed, don't run off
}

static trit_t int21_02(dos_session_t *s, uint16_t *regs) {
    // AH=02h: write char in DL to stdout
    s->putchar_fn((char)(regs[REG_DX] & 0xFF));
    return T_TRUE;
}

trit_t dos_int21h(dos_session_t *s, uint8_t ah, uint16_t *regs) {
    if (!s || !regs || s->region_id < 0) return T_FALSE;
    switch (ah) {
        case 0x09: return int21_09(s, regs);
        case 0x02: return int21_02(s, regs);
        case 0x3D: // open
        case 0x3E: // close
        case 0x3F: // read
        case 0x40: // write
            // Recognized, not implemented in v0. A real port would map
            // these onto k_open/k_read/k_write with 8-bit region buffers.
            return T_UNKNOWN;
        case 0x4C: // exit
            s->terminated = 1;
            return T_TRUE;
        default:
            return T_FALSE; // unsupported function
    }
}

// --- Windows 3.1 provisioning (Phase 2 stubs) ---------------------------
trit_t win31_load_ne(int region_id, const uint8_t *img, uint32_t len,
                     win31_mod_t *out) {
    (void)region_id; (void)img; (void)len; (void)out;
    return T_FALSE; // unimplemented in v0
}

trit_t win31_thunk_16(int selector, uint16_t func) {
    (void)selector; (void)func;
    return T_UNKNOWN; // unimplemented in v0
}
