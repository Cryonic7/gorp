; boot.asm — GorpOS boot sector, Pentium 4 (IA-32) baseline
; Assembler: nasm -f bin boot.asm -o boot.bin   (must be exactly 512 bytes)
;
; Flow (real mode, BIOS):
;   1. Segments/stack, save BIOS boot drive in DL
;   2. Print 'G' via BIOS teletype
;   3. Load 73 sectors (LBA 1..73) to 0x7E00 via INT 13h AH=02h in a
;      CHS loop (1.44MB geometry: 18 spt, 2 heads), 3 retries/sector.
;      Covers 0x7E00..0x10E00, i.e. stage2 + kernel @ 0x10000.
;   4. On success print '+', far-jump to 0x0000:0x7E00 (stage2)
;      On failure print 'E' and halt
;
; NOTE: INT 13h AH=42h (LBA/EDD) was tried first but HANGS on the
; SeaBIOS floppy path (verified: even 1-sector LBA reads never return).
; CHS is the robust choice for floppy boot; EDD remains fine for HDD.
;
; Disk layout produced by build.sh:
;   LBA 0        : this boot sector
;   file+0x200   : stage2  -> loaded to 0x7E00
;   file+0x8400  : kernel  -> loaded to 0x10000

BITS 16
ORG 0x7C00

STAGE2_LOAD_OFF  equ 0x7E00
SECTORS_TO_LOAD  equ 73          ; 36.5KB -> covers 0x7E00..0x11000 incl. kernel @0x10000

start:
    cli
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00
    sti

    mov [boot_drive], dl

    mov si, msg_g
    call print_string

    ; --- CHS loop: load SECTORS_TO_LOAD sectors, LBA 1..N ---
    xor bx, bx
    mov es, bx
    mov bx, STAGE2_LOAD_OFF     ; ES:BX destination
    mov word [lba], 1           ; current LBA (1-based)

.next_sector:
    mov cx, 3                   ; retries
.retry:
    push cx
    push bx
    call lba_to_chs             ; -> CH=cyl, DH=head, CL=sector
    mov ah, 0x02                ; INT 13h: read sectors
    mov al, 1
    mov dl, [boot_drive]
    ; ES:BX already set (preserved across retries via push/pop)
    int 0x13
    pop bx
    pop cx
    jnc .sector_ok
    ; reset disk, retry
    mov ah, 0x00
    mov dl, [boot_drive]
    int 0x13
    loop .retry
    mov si, msg_e               ; out of retries
    call print_string
    jmp halt
.sector_ok:
    add bx, 512                 ; next destination
    jnc .no_wrap                ; BX is 16-bit: carry -> advance ES
    mov ax, es
    add ax, 0x1000              ; +64KB segment
    mov es, ax
.no_wrap:
    inc word [lba]
    cmp word [lba], SECTORS_TO_LOAD + 1
    jb .next_sector

    mov si, msg_ok
    call print_string
    jmp 0x0000:STAGE2_LOAD_OFF

halt:
    cli
    hlt
    jmp halt

; Convert [lba] (1-based) to CHS for 18 spt / 2 heads.
; Out: CH=cylinder low, DH=head, CL=sector (bits 7-6 = cyl high = 0)
lba_to_chs:
    push ax
    push bx
    mov ax, [lba]               ; 0-based LBA index (1 = first sector past boot)
    mov bl, 18
    div bl                      ; AL = LBA/18, AH = LBA%18
    mov cl, ah
    inc cl                      ; sector = rem + 1
    mov ch, al
    shr ch, 1                   ; cylinder = (LBA/18) / 2
    and al, 1
    mov dh, al                  ; head = (LBA/18) % 2
    pop bx
    pop ax
    ret

print_string:                   ; SI -> NUL-terminated string
    push si
.next:
    lodsb
    test al, al
    jz .done
    mov ah, 0x0E
    mov bh, 0x00
    mov bl, 0x07
    int 0x10
    jmp .next
.done:
    pop si
    ret

msg_g  db 'G', 0
msg_ok db '+', 0
msg_e  db 'E', 0

boot_drive db 0
lba dw 0

times 510-($-$$) db 0
dw 0xAA55
