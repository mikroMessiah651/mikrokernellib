[bits 64]


global spinlock_init
global spinlock_acquire
global spinlock_release

section .text

spinlock_acquire:
    ; we just need to deal with interrupts for the processor
    ; [rdi] = lk->ticket, [rdi + 2] = lk->serving,
    ; [rdi + 4] = lk->cpu_id, [rdi + 8] = lk->rflags
    pushfq
    pop rdx
    cli

    push rbx

    mov ax, 1
    lock xadd word [rdi], ax
.spin:
    cmp word [rdi + 2], ax
    je .locked
    pause
    jmp .spin

.locked:
    mov qword [rdi + 8], rdx

    ; put APIC ID in [rdi + 4](lk->cpu_id)
    mov eax, 1
    cpuid
    shr ebx, 24
    and ebx, 0xff
    ; mask the most significant byte
    mov dword [rdi + 4], ebx

    pop rbx
    ret

spinlock_release:
    mov rdx, qword [rdi + 8] ; rdx = lk->rflags

    mov dword [rdi + 4], 0xffffffff ; lk->cpu_id = (uint32_t)-1;
    add word [rdi + 2], 1

    push rdx
    popfq ; restore rflags
    ret


spinlock_init:
    mov word [rdi], 0                  ; lk->ticket = 0
    mov word [rdi + 2], 0              ; lk->serving = 0
    mov dword [rdi + 4], 0xffffffff    ; lk->cpu_id = 0xffffffff
    mov qword [rdi + 8], 0             ; lk->rflags = 0
    ret
