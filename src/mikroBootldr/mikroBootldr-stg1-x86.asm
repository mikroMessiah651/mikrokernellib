; Author: Eyal Kaghanovich
; A 8086 stage 1 bootloader
; µBoot

ORG 0x0000
BITS 16
jmp .start

section .text


.start:

	cld ; clear DF
	cli ; clear int's

	mov ax, 0x7c0 ; 0x7c0 >> 4 = 0x7c00
	mov ds, ax
	mov es, ax ; ds and es point to 0x7c00

	mov ax, 0x8000
	mov ss, ax ; SS = 0x8000
	mov sp, 0x0000 ; stack grows downwards from phys address 0x80000 = 0x8000:0x0000
	mov bp, 0x0000 ; initialize bp aswell
	sti ; reenables int's

	;enable vga graphics
	mov ah, 0x00
	mov al, 0x03
	int 0x10

	mov byte [BootDrive], dl ; store boot drive number in 0x7dfa
	mov ah, 0x41
	mov al, 0x00
	mov bx, 0x55aa
	;mov byte dl, [BootDrive]
	int 0x13 ; if CF set -> err, unsupported, if bx=55aa->unsupported
	jc .lba_unsupported
	cmp bx, 0xaa55
	jne .lba_unsupported
	test cx, 1
	jz .lba_unsupported
	; lba available

.lba_supported:

	; setup es:bx = 0000:7e00
	xor ax, ax
	mov es, ax
	mov bx, 0x7e00

	mov ah, 0x42
	mov al, 0x00
	mov byte dl, [BootDrive]

	mov si, disk_address_packet

	int 0x13
	jc .disk_error
	; by now we loaded stage 2 to ES:BX = 0000:7E00
	; we now need to jump to stage 2 or atleast pass param's and setup stuff
	jmp .jump_stage2


.lba_unsupported:
;get_chs_geometry
	mov ah, 0x08
	mov dl, [BootDrive]
	int 0x13
	jc .use_default_geometry
	; now we need to specify geometry values for .chs_load_stage2
	; store ch which holds the lower 8 bits of the max cylinder number
	mov byte [LowerBits], ch
	; store cl[6:7] which holds the upper bytes
	mov byte [UpperBits], cl
	inc dh
	mov byte [TotalHeads], dh
	dec dh
	; SPT = CL AND 0x3F
	mov dh, cl
	and dh, 0x3f
	mov byte [SectorPerTrack], dh
	; calculate sector
	mov ax, 1 ; AX = LBA number
	div byte [SectorPerTrack]
	inc ah
	mov [Sector], ah
	; Sector = remainder(LBA / SPT)
	; al = quotient(LBA / SPT)
	; calculate Head and Cylinder
	xor ah, ah
	div byte [TotalHeads]
	mov byte [Head], ah ;ah = remainder(quotient(lba / spt) / totalheads)
	mov byte [Cylinder], al

.chs_load_stage2:
	; CHS geometry must be specified in memory

	xor ax, ax
	mov es, ax ; es points to 0x0000
	mov bx, 0x7e00 ; es:bx = 0x0000:0x7e00


	mov byte dh, [Head]
	mov byte ch, [Cylinder]
	mov byte cl, [Sector]
	; sector guaranteed to be smaller than 255 so upperBits not needed

	mov byte dl, [BootDrive] ; specify the drive


	mov al, 0x03 ; How many sectors to read.
	; we need to read the size of our stage 2

	mov ah, 0x02
	int 0x13
	; load sectors of stage 2 bootloader at es:bx = 0000:7e00

	jc .disk_error
	jmp .jump_stage2_CHS
	; jump to stage 2

.use_default_geometry:
	; set up default CHS geometry values for int 13h and jump to .chs_load_stage2
	;mov byte [SectorPerTrack], 63
    ;mov byte [TotalHeads], 255
	mov byte [Head], 0
	mov byte [Cylinder], 0
	mov byte [Sector], 2
	jmp .chs_load_stage2


.jump_stage2_CHS:

	xor dx, dx
	mov dh, 0x01 ; if loaded via CHS dh holds 1 else dh is 0

.jump_stage2:
	; pass parameters and setup registers for stage 2
	; jump to stage 2 code

	mov byte dl, [BootDrive]
	jmp 0x0000:0x7e00 ; far jump to stage 2


.disk_error:
	; we will handle disk errors by sending the program to .use_default_geometry
	; i know, very impressive error handling indeed
	mov di, 0xeffe
	jmp .use_default_geometry


BootDrive: db 0

SectorPerTrack: db 0
LowerBits: db 0
UpperBits: db 0
TotalHeads: db 0
Cylinder: db 0
Sector: db 2
Head: db 0

disk_address_packet: ; DAP

    db 0x10        ; size of packet (16 bytes)
    db 0           ; always 0
    dw 3     ; number of sectors to read
    dw 0x7e00 ; bx
    dw 0x0000 ; es
    dd 0x00000001 ; lower 32 bits of LBA
    dd 0x00000000 ; upper 32 bits of LBA (0 for typical bootloader)


times 510- ($ -$$) db 0
BootSignature: dw 0xAA55 ; Boot signature little endian so aa 55 becomes 55 aa
