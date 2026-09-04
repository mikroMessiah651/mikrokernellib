; Author: Eyal Kaghanovich
; A 8086 stage 2 bootloader
; µBoot


org 0x7e00
bits 16

; Number of 512-byte sectors the kernel occupies on disk.
; The build system overrides this with -DKERNEL_SECTORS=<exact count> computed
; from the real kernel.bin, so the loader always pulls in the whole kernel.
; The default below only applies to a standalone `nasm` invocation.
%ifndef KERNEL_SECTORS
%define KERNEL_SECTORS 127
%endif

; TODO: Change label to not use a dot prefix

jmp .start

section .text

; dl holds BootDrive
; dh holds 1 if we booted from CHS anything else then we booted from LBA
; if di = 0xeffe then we booted from CHS with an error and default geometry
; ds and es point to 0x7c00, ds = es = 0x07c0
; stack setup: ss = 0x8000, sp = 0x0000, bp = 0x0000
; stage 2 loaded at 0x0000:0x7e00, es = 0x0000, bx = 0x7e00
; cs:ip = 0x0000:0x7e00, probably

.start:

    xor ax, ax
    mov ds, ax ; DS inherited from stage1 as 0x07C0 — reset to 0 so [label] = physical label VMA
    mov es, ax

    mov word [DefaultGeometryFlag], di ; if 0xEFFE -> boot with default geometry and CHS error
    mov byte [BootDrive], dl
    mov byte [LoadMode], dh ; 1 if CHS else LBA

    ; load kernel sectors from disk using int 0x13 or LBA
    cmp  dh, 1
    jne  .load_kernel_LBA

.load_kernel_CHS:

    mov ax, 0x1300
    mov es, ax
    mov bx, 0x0000

    mov byte dh, 0x00 ; head
    mov byte ch, 0x00 ; cylinder
    mov byte cl, 0x06
    ; start of our kernel in memory since we use binary concatenation to create a disk image
    mov byte dl, [BootDrive]

    mov al, KERNEL_SECTORS ; whole kernel (build-time computed)

    mov ah, 0x02
	int 0x13
    ; executes the int 0x13

    jmp .load_mmap_BIOS

.load_kernel_LBA:

    mov ax, 0x1300
    mov es, ax
    mov bx, 0x0000

    mov ah, 0x42
    mov al, 0x00
    mov byte dl, [BootDrive]

    mov si, disk_address_packet_kernel

    int 0x13
    jc .disk_err
    jmp .load_mmap_BIOS

.disk_err:

    mov word di, [DefaultGeometryFlag]
    mov byte dl, [BootDrive]
    mov byte dh, [LoadMode]

    jmp .load_kernel_CHS


.load_mmap_BIOS:
    ; load memory map from BIOS
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov di, 0x5000
    ; sets buffer to 0x0000:0x5000

    xor ebx, ebx        ; EBX must be 0 to start
    mov edx, 0x534D4150 ; 'SMAP' signature
    xor bp, bp          ; BP = entry counter

    .e820_loop:

    mov eax, 0xe820
    mov ecx, 24         ; 24 bytes
    int 0x15

    jc .e820_done

    cmp eax, 0x534D4150
    jne .e820_done

    test ebx, ebx       ; EBX = 0 means last entry
    jz .e820_done

    cmp ecx, 0          ; Skip zero-length
    je .e820_loop

    inc bp              ; Count this entry
    add di, 24          ; Move to next entry
    jmp .e820_loop

.e820_done:

    inc bp
    mov word [0x7000], bp    ; Store entry count at 0x7000

; now we have memory map at 0x5000 for the kernel to read

.vesa_bios_extensions:
; now we will enable vesa bios extensions

; AX = 4F00h
; ES:DI -> buffer for SuperVGA information (see #00077)
; Return:
; AL = 4Fh if function supported
; AH = status
; 00h successful
; ES:DI buffer filled
; 01h failed
; from ralf brown's interrupt list

    mov ax, 0x4f00
    xor di, di
    mov es, di
    mov di, 0x3000
    ; es = 0x0000
    ; di = 0x3000
    int 0x10

    cmp ax, 0x004f
    jne vbe_failure

    ; now iterate over the list at [0x3000 + 0x0e]
    ; and look for 1280x720, 32 bytes per pixel,
    ; support for linear framebuffer and memory model 0x06
    mov si, [0x3000 + 0x0e] ; offset half of real-mode ptr
    mov dx, [0x3000 + 0x10] ; segment half

.loop_vbe:
    mov es, dx ; restore es to the segment ptr
    mov word cx, [es:si] ; cx = mode number

    cmp cx, 0xffff
    je vbe_failure
    ; 0xffff is the terminator for the mode list

    xor di, di
    mov es, di
    mov di, 0x3200 ; mode info block for the int 0x10 call
    ; should i zero the mode info block between int 0x10 calls?

    push cx
    mov ax, 0x4f01
    int 0x10

    ; check mode attributes for linear framebuffer support
    ; basically means to check if bit 7 is set in 0x3200 + 0x00
    bt word [es:di + 0x00], 7
    jnc .skip_vbe_mode

    cmp word [es:di + 0x12], VESA_WIDTH
    jne .skip_vbe_mode
    cmp word [es:di + 0x14], VESA_HEIGHT
    jne .skip_vbe_mode

    cmp byte [es:di + 0x19], 32
    jne .skip_vbe_mode

    cmp byte [es:di + 0x1b], 0x06
    jne .skip_vbe_mode

.found_vbe_mode:
    pop cx
    ; need to save pitch and framebuffer physical address
    ; offsets 0x10(word), 0x28(dword)
    mov dword eax, [es:di + 0x28]
    mov dword [0x7100], eax ;fb

    mov ax, [es:di + 0x10]
    mov word [0x7104], ax ;pitch

    ; now save height, width, bpp to vbe handoff
    mov ax, [es:di + 0x12]
    mov word [0x7106], ax ;width

    mov ax, [es:di + 0x14]
    mov word [0x7108], ax ;height

    mov al, [es:di + 0x19] ;bpp
    mov byte [0x710a], al

    jmp .vbe_set_mode

.skip_vbe_mode:
    pop cx
    add si, 0x02
    jmp .loop_vbe

.vbe_set_mode:
    ; we need to actually set the mode whose number is specified in si register
    mov ax, 0x4F02
    mov bx, cx
    or  bx, 0x4000      ; set bit 14 for linear framebuffer shit
    int 0x10

    cmp ax, 0x004f
    jne vbe_failure

    xor ax, ax
    mov es, ax

.a20:
    ;now our task is to enable the A20 line and load a gdt and transition to protected mode
    ; ax = 0
    call test_a20
    ; ax = 0 -> disabled A20, ax = 1 -> enabled A20
    cmp ax, 0
    jne enabled_a20

enable_a20_bios:

    mov ax, 0x2401          ; Enable A20 function
    int 0x15                ; BIOS interrupt
    jnc enabled_a20
    ; if carry flag is set than there is an error and we should use fast A20
    call load_fast_a20

enabled_a20:

    xor ax, ax
    mov ds, ax
    mov es, ax
    ; now we need to load the gdt and enter protected mode
    cli
    lgdt [gdt_descriptor]
    ; load gdt
    mov eax, cr0
    or eax, 1
    mov cr0, eax
    ; enable protected mode
    jmp CODE_SEG32:protected_mode_start
    ; far jmp to 32 bit code

load_fast_a20:
    in al, 0x92             ; Read System Control Port A
    test al, 2              ; Check if already enabled
    jnz enabled_a20
    or al, 2                ; Set bit 1
    and al, 0xFE            ; Clear bit 0 (don't reset system!)
    out 0x92, al            ; Write back
    ret

test_a20:

    pushf
    push ds
    push si
    push es
    push di

    cli
    xor ax, ax
    mov es, ax
    mov di, 0x0500

    mov ax, 0xffff
    mov ds, ax
    mov si, 0x0510

    mov al, byte [es:di]
    push ax

    mov al, byte [ds:si]
    push ax

    mov byte [es:di], 0x00
    mov byte [ds:si], 0xff
    cmp byte [es:di], 0xff

    pop ax
    mov byte [ds:si], al
    pop ax
    mov byte [es:di], al

    sti
    xor ax, ax
    je .exit
    mov ax, 1
.exit:

    pop di
    pop es
    pop si
    pop ds
    popf
    ret

vbe_failure:
; TODO add actual error handling
    cli
    jmp $


[bits 32]
protected_mode_start:
    ; setup data segment registers
    mov ax, DATA_SEG32; Load data segment selector
    mov ds, ax
    mov ss, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    ; setting up stack
    mov esp, 0x90000
    mov ebp, 0
    ; now check for cpuid support for long mode

check_cpuid:

    pushfd ; push EFLAGS
    pop eax ; put EFLAGS in eax

    mov ecx, eax ;store original EFLAGS value
    xor eax, EFLAGS_ID
    ; storing the eflags and then retrieving it again will show whether or not
    ; the bit could successfully be flipped
    push eax
    popfd
    pushfd
    pop eax
    ; we put modified EFLAGS in EFLAGS
    ; we now need to restore EFLAGS in ecx and check if eax equals ecx
    push ecx
    popfd
    ; if eax == ecx -> uncsuccesfully flipped -> not supported
    xor eax, ecx
    jz long_mode_unsupported
    ; if we got here CPUID instruction supported
    ; we need to check if the extended function is supported
query_long_mode:
    ; query 1gb pages
    ; for now 1gb pages are a must for this to boot, for i am using them in the 4GB identity map for low memory
    push edx

    mov eax, 0x80000000
    cpuid

    cmp eax, 0x80000001
    jb long_mode_unsupported

    mov eax, 0x80000001
    cpuid

    bt edx, 26
    setc al
    movzx eax, al

    cmp eax, 1
    jne long_mode_unsupported

    pop edx

    mov eax, CPUID_EXTENSIONS
    cpuid
    cmp eax, CPUID_EXT_FEATURES
    jb long_mode_unsupported
    ; if we got here, we can check for long mode support
    mov eax, CPUID_EXT_FEATURES
    cpuid
    test edx, CPUID_EDX_EXT_FEAT_LM
    jz long_mode_unsupported

setup_paging:
    mov edi, PML4T_ADDR
    mov cr3, edi
   
    xor eax, eax

    mov ecx, 1024 * 5
    rep stosd ; zero the page tables, 5 because there are 2 PDPTs


    ; identity map 4 GB using huge(1 GB) pages at the pdpt level
    mov dword [PML4T_ADDR + 0 * 8], PDPT_ADDR | PTE_PRESENT | PTE_WRITABLE ; mov pml4t[0], (uint64_t)(pdpt | PTE_PRESENT | PTE_WRITABLE)
    mov dword [PML4T_ADDR + 4], 0

    mov dword [PDPT_ADDR + 0 * 8], 0 | PTE_PRESENT | PTE_WRITABLE | PTE_PS      ; 1GB
    mov dword [PDPT_ADDR + 4], 0 ; mov pdpt[0], 0 | PTE_WRITABLE | PTE_PRESENT | PTE_PS
    
    mov dword [PDPT_ADDR + 1 * 8], 0x40000000 | PTE_PRESENT | PTE_WRITABLE | PTE_PS ; 2GB
    mov dword [PDPT_ADDR + 12], 0

    mov dword [PDPT_ADDR + 2 * 8], 0x80000000 | PTE_PRESENT | PTE_WRITABLE | PTE_PS  ; 3GB
    mov dword [PDPT_ADDR + 2 * 8 + 4], 0

    mov dword [PDPT_ADDR + 3 * 8], 0xc0000000 | PTE_PRESENT | PTE_WRITABLE | PTE_PS ; 4GB
    mov dword [PDPT_ADDR + 3 * 8 + 4], 0

map_kernel_half:
    ; map high addresses to low kernel addresses
    ; for high symbols to work with low code
    ; then make the linker script link for hig addresses
    ; use 4kb PT pages, indices are: pml4t[511], pdpt[510], pd[0], pt[0]
    mov dword [PML4T_ADDR + 511 * 8], SECOND_PDPT_ADDR | PTE_PRESENT | PTE_WRITABLE
    mov dword [PML4T_ADDR + 511 * 8 + 4], 0

    mov dword [SECOND_PDPT_ADDR + 510 * 8], PD_ADDR | PTE_PRESENT | PTE_WRITABLE
    mov dword [SECOND_PDPT_ADDR + 510 * 8 + 4], 0

    mov dword [PD_ADDR], PT_ADDR | PTE_PRESENT | PTE_WRITABLE
    mov dword [PD_ADDR + 4], 0
    
    push edx
    push eax
    push ecx

    xor ecx, ecx
    xor edx, edx
    mov edx, 0 | PTE_PRESENT | PTE_WRITABLE | PTE_NX
    mov eax, 0
cover_pt_loop:
    push eax
    or eax, edx

    mov dword [PT_ADDR + ecx * 8], eax
    mov dword [PT_ADDR + ecx * 8 + 4], 0
    
    pop eax
    add eax, 0x1000
    inc ecx
    cmp ecx, 512
    jne cover_pt_loop
    ; loop covers pt[0..511] as needed

    pop ecx
    pop eax
    pop edx


transition_to_lm:

    mov eax, cr4
    or eax, 0x20 ; 00100000b
    mov cr4, eax
    ; loads cr4 and writes the PAE bit to cr4

    lgdt [gdt_descriptor64]
    ;loads the 64 bit gdt

    ; set EFER.LME and EFER.NXE, 8th and 11th bits
    mov ecx, 0xC0000080
    rdmsr
    or eax, (1 << 8) | (1 << 11)
    wrmsr

    ; activate paging
    mov eax, cr0
    or eax, (1 << 31)
    mov cr0, eax

    ; far jump to 64 bits code
    jmp CODE_SEG64:long_mode_start

long_mode_unsupported:
    jmp $


[bits 64]
long_mode_start:
    ; setup segment registers
    mov ax, DATA_SEG32 ; Load data segment selector (64bit)
    mov ds, ax
    mov ss, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    mov rsp, 0x90000
    mov rbp, 0

    ; Clear registers
    xor rdi, rdi
    xor rsi, rsi
    xor rdx, rdx
    xor rcx, rcx
    xor r8, r8
    xor r9, r9

    ; jmp to kernel
    ; jmp 0x13000
    jmp KERNEL_IMAGE_START + KERNEL_PHYS_START
    cli
    hlt
    jmp $


; protected mode variables
gdt_start64:
    ; Null descriptor (required)
    dq 0x0000000000000000

gdt_code64:

    dw 0x0000       ; Limit
    dw 0x0000       ; Base (low)
    db 0x00         ; Base (mid)
    db 10011010b    ; Access
    db 00100000b    ; Granularity (Long mode)
    db 0x00         ; Base (high)
gdt_data64:

    dw 0x0000
    dw 0x0000
    db 0x00
    db 10010010b    ; Access
    db 0x00
    db 0x00
gdt_end64:

gdt_descriptor64:

    dw gdt_end64 - gdt_start64 - 1 ; Size
    dd gdt_start64                 ; Offset

CODE_SEG64 equ gdt_code64 - gdt_start64
DATA_SEG64 equ gdt_data64 - gdt_start64


EFLAGS_ID equ 1 << 21

CPUID_EXTENSIONS equ 0x80000000 ; returns the maximum extended requests for cpuid
CPUID_EXT_FEATURES equ 0x80000001 ; returns flags containing long mode support among other things
CPUID_EDX_EXT_FEAT_LM equ 1 << 29   ; if this is set, the CPU supports long mode

PML4T_ADDR        equ 0x9000
PDPT_ADDR         equ 0xa000
SECOND_PDPT_ADDR  equ 0xb000
PD_ADDR           equ 0xc000
PT_ADDR           equ 0xd000

PTE_PRESENT        equ 1          ; page present
PTE_WRITABLE       equ 2          ; page read/write
PTE_PS             equ (1 << 7)   ; 2MB huge page (Page Size bit in PDT entry)
PTE_NX             equ 0x8000000000000000

KERNEL_IMAGE_START equ 0xFFFFFFFF80000000
KERNEL_PHYS_START equ 0x13000

SIZEOF_PTE        equ 8
PAGE_TABLE_DWORDS equ 1024       ; one 4KB page table = 1024 dwords

MMAP_ENTRIES      equ 0x5000
MMAP_COUNT        equ 0x7000

; real mode variables
disk_address_packet_kernel:

    db 0x10        ; size of packet (16 bytes)
    db 0           ; always0
    dw KERNEL_SECTORS    ; num of sectors to read (whole kernel, build-time computed)
    dw 0x0000 ; bx
    dw 0x1300 ; es
    dd 0x00000005 ; lower 32 bits
    dd 0x00000000 ; upper 32 bits

gdt_start:
    ; Null descriptor (required)
    dq 0x0000000000000000

gdt_code32:
    ; Code segment: base=0, limit=0xFF
    dw 0xFFFF       ; Limit 0-15
    dw 0x0000       ; Base 0-15
    db 0x00         ; Base 16-23
    db 10011010b    ; Access: present, ring 0, code, executable, readable
    db 11001111b    ; Flags: 4KB granularity, 32-bit Limit 16-19
    db 0x00         ; Base 24-31

gdt_data32:
    ; Data segment: base=0, limit=0xFFFFF, 4KB granularity
    dw 0xFFFF       ; Limit 0-15
    dw 0x0000       ; Base 0-15
    db 0x00         ; Base 16-23
    db 10010010b    ; Access: present, ring 0, data, writable
    db 11001111b    ; Flags: 4KB granularity, 32-bit Limit 16-19
    db 0x00         ; Base 24-31
gdt_end:

gdt_descriptor:
    dw gdt_end - gdt_start - 1  ; Size
    dd gdt_start                 ; Offset

CODE_SEG32 equ gdt_code32 - gdt_start  ; 0x08
DATA_SEG32 equ gdt_data32 - gdt_start  ; 0x10

BootDrive: db 0x00
LoadMode: db 0x00
DefaultGeometryFlag: dw 0x0000

times 1534- ($ -$$) db 0
kernelSignature: dw 0xffff
