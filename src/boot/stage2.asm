; stage2.asm — GorpOS stage 2, Pentium 4 (IA-32) baseline
; Loaded by boot.asm to 0x7E00. Flat binary: 16-bit prologue followed
; by 32-bit protected-mode code.
;
; Build: nasm -f bin stage2.asm -o stage2.bin
;
; 16-bit (real mode):
;   print '2', enable A20, lgdt, set CR0.PE, far-jump to 32-bit
; 32-bit:
;   segments/stack, identity paging (PD 0x7000, PT 0x8000, 0-4MB),
;   enable CR0.PG, VGA print "P4", QEMU debug-exit 0x41, jump to
;   kernel at 0x10000.
;
; Memory map used here (all < 1MB, identity-mapped):
;   0x5000 : page directory (4KB) — 0x7000/0x8000 are BIOS scratch, avoid
;   0x6000 : page table     (4KB)
;   0x9000 : stack top (grows down)
;   0x7E00 : this stage2
;   0xB8000: VGA text
;   0x10000: kernel (linked ENTRY kmain)

BITS 16
ORG 0x7E00

PD_ADDR  equ 0x5000
PT_ADDR  equ 0x6000
STACK_TOP equ 0x9000
KERNEL_ADDR equ 0x10000
VGA_BASE equ 0xB8000

stage2_16:
    ; NOTE: no BIOS video calls here. INT 10h breaks subsequent port I/O
    ; on QEMU 9.2.1 (the VGA BIOS leaves the I/O subsystem in a state
    ; where OUT to the debug-exit port hangs). We print via VGA directly
    ; once in protected mode.

    ; --- enable A20: try BIOS, fall back to port 0x92 ---
    mov ax, 0x2401
    int 0x15
    jnc .a20_done
    in al, 0x92
    or al, 0x02
    out 0x92, al
.a20_done:

    cli
    lgdt [gdt_desc]
    mov eax, cr0
    or eax, 0x1                 ; CR0.PE
    mov cr0, eax
    jmp 0x08:pm32               ; far jump: flush pipeline, load CS

; ---------------- 32-bit protected mode ----------------
BITS 32
pm32:
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    mov esp, STACK_TOP

    ; --- identity paging: PD[0] -> PT, PT identity-maps 0..4MB ---
    mov edi, PD_ADDR
    xor eax, eax
    mov ecx, 1024
    rep stosd                   ; zero page directory
    mov edi, PT_ADDR
    mov ecx, 1024
    rep stosd                   ; zero page table
    mov dword [PD_ADDR], PT_ADDR | 0x3   ; present, writable
    mov edi, PT_ADDR
    mov eax, 0x3                ; present, writable
    mov ecx, 1024
.pt_fill:
    mov [edi], eax
    add eax, 0x1000
    add edi, 4
    loop .pt_fill

    mov eax, PD_ADDR
    mov cr3, eax
    mov eax, cr0
    or eax, 0x80000000          ; CR0.PG
    mov cr0, eax
    jmp 0x08:.pg_on
.pg_on:

    ; --- VGA: print "P4" top-left ---
    mov edi, VGA_BASE
    mov ax, 0x0F50              ; 'P', white on black
    mov [edi], ax
    mov ax, 0x0F34              ; '4', white on black
    mov [edi+2], ax

    ; --- hand off to kernel ---
    jmp 0x08:KERNEL_ADDR

; ---------------- GDT ----------------
align 8
gdt:
    dq 0x0000000000000000        ; 0x00 null
    dq 0x00CF9A000000FFFF        ; 0x08 code32: base 0, 4GB, exec/read
    dq 0x00CF92000000FFFF        ; 0x10 data32: base 0, 4GB, read/write
gdt_end:
gdt_desc:
    dw gdt_end - gdt - 1
    dd gdt

; NOTE: no IDT yet — interrupts stay disabled (cli). Any interrupt or
; exception in protected mode right now triple-faults. IDT + PIC remap
; is the next bring-up step (see docs/ROADMAP.md).
