// io.h — port I/O primitives (Pentium 4 / IA-32 baseline)
//
// IN/OUT instructions are still valid on Pentium 4, so these work
// unchanged from the 8086 originals. They also compile on x86_64 hosts
// (IN/OUT are still valid there), so host syntax-checks work — but
// actually EXECUTING them on a host needs I/O privilege; the host test
// build never calls them.
//
// Note: on future AArch64/RISC-V ports these do NOT exist — see
// docs/hardware/porting.md (MMIO instead).

#ifndef IO_H
#define IO_H

#include <stdint.h>

static inline uint8_t inb(uint16_t port) {
    uint8_t v;
    __asm__ volatile("inb %w1, %b0" : "=a"(v) : "Nd"(port));
    return v;
}

static inline void outb(uint16_t port, uint8_t v) {
    __asm__ volatile("outb %b0, %w1" : : "a"(v), "Nd"(port));
}

static inline uint16_t inw(uint16_t port) {
    uint16_t v;
    __asm__ volatile("inw %w1, %w0" : "=a"(v) : "Nd"(port));
    return v;
}

static inline void outw(uint16_t port, uint16_t v) {
    __asm__ volatile("outw %w0, %w1" : : "a"(v), "Nd"(port));
}

// Tiny I/O delay (writes to the unused 0x80 debug port, classic trick)
static inline void io_wait(void) {
    outb(0x80, 0);
}

#endif
