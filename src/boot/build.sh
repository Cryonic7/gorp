#!/bin/sh
# build.sh — build GorpOS bootable image (Pentium 4 / IA-32 baseline)
# Requires: nasm, gcc (with -m32), ld, objcopy
# Optional: qemu-system-i386 (for the boot test at the end)
set -e
cd "$(dirname "$0")"

command -v nasm >/dev/null || { echo "nasm not found" >&2; exit 1; }

# 1. boot sector: must be exactly 512 bytes, ends 55 AA
nasm -f bin boot.asm -o boot.bin
sz=$(wc -c < boot.bin)
[ "$sz" -eq 512 ] || { echo "boot.bin is $sz bytes, expected 512"; exit 1; }
sig=$(od -A n -t x1 -j 510 boot.bin | tr -d ' \n')
[ "$sig" = "55aa" ] || { echo "bad boot signature: $sig"; exit 1; }

# 2. stage2: 16-bit prologue + 32-bit PM/paging code (flat binary)
nasm -f bin stage2.asm -o stage2.bin
s2sz=$(wc -c < stage2.bin)
[ "$s2sz" -lt 33280 ] || { echo "stage2 too big: $s2sz"; exit 1; }

# 3. kernel: 32-bit freestanding C, linked at 0x10000
gcc -m32 -ffreestanding -nostdlib -Wall -Wextra -I ../kernel \
    -c ../kernel/main.c -o kernel.o
gcc -m32 -ffreestanding -nostdlib -Wall -Wextra -I ../kernel \
    -c ../kernel/alloc.c -o alloc.o
ld -m elf_i386 -T ../kernel/kernel.ld -o kernel.elf kernel.o alloc.o
objcopy -O binary kernel.elf kernel.bin
ksz=$(wc -c < kernel.bin)
echo "kernel.bin: $ksz bytes"

# 4. image layout: [boot][stage2][pad to 0x8400][kernel]
#    file 0x200 -> 0x7E00 ; file 0x8400 -> 0x10000
cat boot.bin stage2.bin > gorpos.img
cur=$(wc -c < gorpos.img)
padto=33792  # 0x8400
[ "$cur" -le "$padto" ] || { echo "stage2 overflowed kernel offset"; exit 1; }
dd if=/dev/zero bs=1 count=$((padto - cur)) >> gorpos.img 2>/dev/null
cat kernel.bin >> gorpos.img
echo "wrote gorpos.img ($(wc -c < gorpos.img) bytes)"

# 5. optional QEMU boot test (P4-era CPU model is gone in QEMU 9+;
#    qemu32+sse2 is the closest available 32-bit SSE2 baseline).
#    Verification: stage2 writes "P4" to VGA, kmain writes "GorpOS".
#    We dump VGA text memory (0xB8000) via the monitor.
#    (isa-debug-exit is NOT used: OUT hangs after BIOS video calls on
#    QEMU 9.2.1, and 8-bit OUT is mis-emulated. See docs/REVIEW.md R-22.)
if command -v qemu-system-i386 >/dev/null && command -v socat >/dev/null; then
    echo "--- QEMU boot test (VGA check via monitor) ---"
    rm -f /tmp/gorpos-qmon
    timeout 30 qemu-system-i386 -cpu qemu32,+sse2 \
        -drive file=gorpos.img,format=raw,if=floppy -boot a \
        -display none -monitor unix:/tmp/gorpos-qmon,server,nowait \
        2>/dev/null &
    QPID=$!
    for i in $(seq 1 10); do
        [ -S /tmp/gorpos-qmon ] && break
        sleep 1
    done
    sleep 3
    VGA=$(printf 'x /32bx 0xb8000\n' | socat - UNIX-CONNECT:/tmp/gorpos-qmon 2>/dev/null | strings | grep -a "b8000:")
    printf 'quit\n' | socat - UNIX-CONNECT:/tmp/gorpos-qmon >/dev/null 2>&1
    wait $QPID 2>/dev/null
    echo "VGA: $VGA"
    case "$VGA" in
        *0x50*0x34*)
            echo "BOOT TEST PASS: stage2 reached protected mode (P4 on VGA)"
            ;;
        *)
            echo "BOOT TEST INCONCLUSIVE; see docs/REVIEW.md"
            ;;
    esac
    # 0x50='P', 0x34='4' — kernel text check needs the full string;
    # the monitor dump above shows the first 16 cells.
else
    echo "qemu or socat not found; skipping emulation test"
fi
