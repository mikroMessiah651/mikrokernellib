; Author: Eyal Kaghanovich
; A 8086 stage 2 bootloader



org 0x7e00
bits 16

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
    mov ds, ax  ; DS inherited from stage1 as 0x07C0 — reset to 0 so [label] = physical label VMA
    mov es, ax

    mov word [DefaultGeometryFlag], di ; if 0xEFFE -> boot with default geometry and CHS error
    mov byte [BootDrive], dl
    mov byte [LoadMode], dh ; 1 if CHS else LBA

    ; load kernel sectors from disk using int 0x13 or LBA
    cmp dh, 1
    jne .load_kernel_LBA

.load_kernel_CHS:

    mov ax, 0x1300
    mov es, ax
    mov bx, 0x0000

    mov byte dh, 0x00 ; head
    mov byte ch, 0x00 ; cylinder
    mov byte cl, 0x06
    ; start of our kernel in memory since we use binary concatenation to create a disk image
    mov byte dl, [BootDrive]

    mov al, 0x0D ; 13 sectors (stays within track 0 from CHS sector 6 to 18)
    
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

    mov eax, 0xE820
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

a20:
    ;now our task is to enable the A20 line and load a gdt and transition to protected mode
    xor ax, ax
    call .test_A20
    ; ax = 0 -> disabled A20, ax = 1 -> enabled A20
    cmp ax, 0
    jne .enabled_A20

.enable_A20_bios:

    mov ax, 0x2401          ; Enable A20 function
    int 0x15                ; BIOS interrupt
    jnc .enabled_A20
    ; if carry flag is set than there is an error and we should use fast A20
    call .load_fast_A20

.enabled_A20:

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

.load_fast_A20:

    in al, 0x92             ; Read System Control Port A
    test al, 2              ; Check if already enabled
    jnz .enabled_A20
    or al, 2                ; Set bit 1
    and al, 0xFE            ; Clear bit 0 (don't reset system!)
    out 0x92, al            ; Write back
    ret


.test_A20:

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
    
    xor ebx, ebx ; offset to write at: 0x0000
    mov edx, pm_msg
    call println_32bit

    
.check_CPUID:

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
    jz .long_mode_unsupported
    ; if we got here CPUID instruction supported
    ; we need to check if the extended function is supported
.query_long_mode:

    mov eax, CPUID_EXTENSIONS
    cpuid
    cmp eax, CPUID_EXT_FEATURES
    jb .long_mode_unsupported
    ; if we got here, we can check for long mode support
    mov eax, CPUID_EXT_FEATURES
    cpuid
    test edx, CPUID_EDX_EXT_FEAT_LM
    jz .long_mode_unsupported

    ; if we got here, long mode is supported
    mov ebx, 0x00A0
    mov edx, lng_mode_supported
    call println_32bit

.setup_paging:
    ; Zero all page table space: PML4T + PDPT + MAX_PDTS PDTs
    mov edi, PML4T_ADDR
    mov cr3, edi
    xor eax, eax
    mov ecx, (2 + MAX_PDTS) * PAGE_TABLE_DWORDS
    rep stosd

    ; PML4T[0] -> PDPT
    mov dword [PML4T_ADDR], PDPT_ADDR | PT_PRESENT | PT_WRITABLE

    ; Walk entire e820 map to find highest physical address
    mov esi, MMAP_ENTRIES
    movzx ecx, word [MMAP_COUNT]
    xor ebx, ebx             ; highest addr low
    xor edx, edx             ; highest addr high

.find_max_addr:
    ; end = base + length (64-bit)
    mov eax, [esi]
    add eax, [esi + 8]
    mov edi, [esi + 4]
    adc edi, [esi + 12]
    ; if edi:eax > edx:ebx, update max
    cmp edi, edx
    ja .update_max
    jb .next_e820
    cmp eax, ebx
    jbe .next_e820
.update_max:

    mov edx, edi
    mov ebx, eax
.next_e820:

    add esi, 24
    dec ecx
    jnz .find_max_addr

    ; edx:ebx = highest physical address from e820
    ; Number of 2MB pages = ceil(highest / 2MB)
    add ebx, 0x1FFFFF
    adc edx, 0
    shrd ebx, edx, 21
    shr edx, 21
    ; ebx = total 2MB pages needed
    ; Number of PDTs = ceil(num_2mb_pages / 512)
    add ebx, 511
    shr ebx, 9

    ; Clamp to [1, MAX_PDTS] — at least 1 for kernel/VGA
    test ebx, ebx
    jnz .cap_pdts
    inc ebx
.cap_pdts:

    cmp ebx, MAX_PDTS
    jbe .pdts_ok
    mov ebx, MAX_PDTS
.pdts_ok:

    push ebx

    ; Wire PDPT[0..n-1] -> PDT[0..n-1]
    mov edi, PDPT_ADDR
    mov eax, PDT_BASE | PT_PRESENT | PT_WRITABLE
    mov ecx, ebx
.wire_pdpt:

    mov [edi], eax
    add eax, 0x1000
    add edi, SIZEOF_PT_ENTRY
    dec ecx
    jnz .wire_pdpt

    ; Fill all PDT entries with 2MB identity-map huge pages
    pop ecx
    shl ecx, 9               ; total entries = num_pdts * 512
    mov edi, PDT_BASE
    mov ebx, PT_PRESENT | PT_WRITABLE | PT_PS
    xor ebp, ebp             ; high 32 bits of physical frame address
.fill_pdts:

    mov [edi], ebx
    mov [edi + 4], ebp
    add ebx, 0x200000
    adc ebp, 0
    add edi, SIZEOF_PT_ENTRY
    dec ecx
    jnz .fill_pdts

    mov edx, lng_mode_enabled
    mov ebx, 0x0140
    call println_32bit

    mov eax, cr4
    or eax, 0x20 ; 00100000b
    mov cr4, eax
    ; loads cr4 and writes the PAE bit to cr4

    lgdt [gdt_descriptor64]
    ;loads the 64 bit gdt
    
    ; set EFER.LME
    mov ecx, 0xC0000080
    rdmsr
    or eax, (1 << 8)
    wrmsr

    ; activate paging
    mov eax, cr0
    or eax, (1 << 31)
    mov cr0, eax

    ; far jump to 64 bits code
    jmp CODE_SEG64:long_mode_start

.long_mode_unsupported:
    ; call println_32bit...
    mov edx, lng_mode_err
    call println_32bit
    jmp $



[bits 64]


long_mode_start:
    ; setup data segment registers
    mov ax, DATA_SEG32 ; Load data segment selector (64bit)
    mov ds, ax
    mov ss, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    mov rsp, 0x90000
    mov rbp, 0

    cmp byte [LoadMode], 0x01
    jne print_lm
    mov rdi, lba_msg
    mov rsi, 8
    mov rdx, 0
    call println_64bit


print_lm:
    mov rdi, lm_msg     ; pointer to string
    mov rsi, 2          ; row 2
    mov rdx, 0          ; col 0
    call println_64bit

    ; Clear registers
    xor rdi, rdi
    xor rsi, rsi
    xor rdx, rdx
    xor rcx, rcx
    xor r8, r8
    xor r9, r9
    
    ; jmp to kernel
    jmp 0x13000
    hlt
    jmp $

; functions


; println_64
; inputs:  rdi = pointer to null-terminated ASCII string
;          rsi = row (0-24)
;          rdx = col (0-79)
; outputs: characters written to VGA text buffer at the given row/col
; clobbers: nothing (all registers preserved via push/pop)
println_64bit:

    push rax
    push rbx
    push rcx
    push rdi
    push rsi
    push rdx

    ; calculate VGA buffer address for (row, col)
    ; each row is 80 cells, each cell is 2 bytes: (row*80 + col) * 2
    mov rax, rsi            ; rax = row
    mov rcx, 80
    mul rcx                 ; rax = row * 80
    add rax, rdx            ; rax = row * 80 + col
    shl rax, 1              ; rax = (row * 80 + col) * 2  (byte offset)
    mov rbx, 0xb8000
    add rbx, rax            ; rbx = pointer into VGA buffer

    xor rcx, rcx            ; rcx = character index
.loop:

    mov al, [rdi + rcx]     ; load next character
    test al, al             ; null terminator?
    jz .done
    mov ah, 0x0f            ; attribute: white on black
    mov [rbx + rcx * 2], ax ; write char + attribute
    inc rcx
    jmp .loop

.done:

    pop rdx
    pop rsi
    pop rdi
    pop rcx
    pop rbx
    pop rax
    ret


; println_32bit
; inputs: edx = pointer to null-terminated string
; clobbers: nothing (all registers preserved)
; ebx: offset from 0xb8000 to write at
[bits 32]
println_32bit:

    pushad
    mov ah, 0x0f            ; attribute: white on black
    xor ecx, ecx            ; ecx = character index
    mov edi, 0xb8000        ; VGA text buffer base
    add edi, ebx
.loop:

    mov al, [edx + ecx]     ; load next character
    test al, al             ; null terminator?
    jz .done
    mov [edi + ecx * 2], ax ; write char + attribute as a word
    inc ecx
    jmp .loop

.done:

    popad
    ret


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
PDT_BASE          equ 0xb000     ; PDTs allocated consecutively from here
MAX_PDTS          equ 8          ; space for 8 PDTs (8GB) before kernel at 0x13000

PT_PRESENT        equ 1          ; page present
PT_WRITABLE       equ 2          ; page read/write
PT_PS             equ (1 << 7)   ; 2MB huge page (Page Size bit in PDT entry)

SIZEOF_PT_ENTRY   equ 8
PAGE_TABLE_DWORDS equ 1024       ; one 4KB page table = 1024 dwords

MMAP_ENTRIES      equ 0x5000
MMAP_COUNT        equ 0x7000


pm_msg: db "Protected mode OK", 0
lng_mode_err: db "long mode unsupported error", 0
lng_mode_supported: db "long mode supported", 0
lng_mode_enabled: db "long mode enabled", 0
lm_msg: db "in 64 bit long mode with paging", 0
lba_msg: db "loaded via LBA", 0
; real mode variables

disk_address_packet_kernel:

    db 0x10        ; size of packet (16 bytes)
    db 0           ; always0
    dw 64    ; num of sectors to read
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
kernelSignature: dw 0xffff ; ff = 11111111b