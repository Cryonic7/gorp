// dos_compat.h — provisioning for DOS binary support (target 1)
// Strategy: NOT native DOS. We provide a loader + INT 21h shim that
// translates DOS calls into our kernel calls, on 8086 real mode.
//
// Phase 1 (DOS):
//  - Loader for .COM (raw, ORG 0x100) and MZ .EXE (relocations)
//  - CPU is already 8086 real-mode, so instructions run natively;
//    only OS services are emulated.
//  - INT 21h handler: catch AH=09h (print string), 02h (print char),
//    3Dh/3Eh/3Fh/40h (file, stubs), 4Ch (exit), translate to k_* calls.
//  - Our packed 8/10/13-bit memory: DOS programs see a flat 8-bit
//    view (width=8 region) for compatibility; kernel keeps its own
//    widths elsewhere.
//
// The loader ALLOCATES its own 8-bit region (byte-aligned) so the
// DOS linear address (seg*16+off) maps 1:1 onto cell indices.
// Callers must NOT pre-allocate the region.
//
// Phase 2 (Windows 3.1, later):
//  - NE executable loader (much bigger job)
//  - 16-bit protected mode / DPMI-ish, USER/GDI/KERNEL shims
//  - Explicitly out of scope for v0; header reserves the ABI.

#ifndef DOS_COMPAT_H
#define DOS_COMPAT_H

#include <stdint.h>
#include "trit.h"
#include "alloc.h"

#define DOS_COM_LOAD_ADDR 0x100

typedef enum {
    BIN_RAW_COM = 0,
    BIN_MZ_EXE  = 1,
    BIN_NE_WIN31 = 2, // reserved, not implemented in v0
} bin_type_t;

// MZ header (packed, little-endian)
typedef struct __attribute__((packed)) {
    uint16_t sig;      // 0x5A4D "MZ"
    uint16_t last_bytes;
    uint16_t blocks;
    uint16_t relocs;
    uint16_t hdr_paras;
    uint16_t min_extra;
    uint16_t max_extra;
    uint16_t ss, sp, checksum, ip, cs, reloc_tab, overlay;
} mz_header_t;

// A loaded DOS program: owns its 8-bit region inside the allocator.
typedef struct {
    allocator_t *a;
    int          region_id;   // 8-bit region holding the image
    bin_type_t   type;
    uint16_t     cs, ip;      // entry point (segment relative, flat model)
    uint8_t      terminated;  // set by INT 21h AH=4Ch
    void       (*putchar_fn)(char); // output hook (host: putchar; hw: BIOS)
} dos_session_t;

// Detected binary type from first bytes
bin_type_t dos_detect(const uint8_t *img, uint32_t len);

// Load .COM / MZ .EXE: allocates an 8-bit region, copies image,
// applies MZ relocations. Fills out session (region_id, cs, ip).
// Returns trit: T_TRUE=ok, T_FALSE=bad format, T_UNKNOWN=needs more memory
trit_t dos_load(allocator_t *a, const uint8_t *img, uint32_t len,
                dos_session_t *out);

// Release the session's region back to the allocator.
void dos_unload(dos_session_t *s);

// INT 21h shim. regs layout: [0]=AX [1]=BX [2]=CX [3]=DX
//                             [4]=SI [5]=DI [6]=DS [7]=ES
// Returns trit: T_TRUE=handled, T_FALSE=unsupported function,
//             T_UNKNOWN=recognized but unimplemented (file IO stubs)
trit_t dos_int21h(dos_session_t *s, uint8_t ah, uint16_t *regs);

// --- Windows 3.1 provisioning (Phase 2, stubs) ---
// We reserve the interface now so kernel revisions can grow into it
// without breaking the DOS path.

typedef struct {
    uint8_t  reserved[64]; // NE header / module table placeholder
} win31_mod_t;

trit_t win31_load_ne(int region_id, const uint8_t *img, uint32_t len,
                     win31_mod_t *out); // v0: returns T_FALSE (unimplemented)
trit_t win31_thunk_16(int selector, uint16_t func); // v0: T_UNKNOWN

#endif
