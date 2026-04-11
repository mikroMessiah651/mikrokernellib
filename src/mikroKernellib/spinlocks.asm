[bits 64]


global spinlock_init
global spinlock_acquire
global spinlock_release

section .text


spinlock_acquire:
    ; when called, this function should have a 64 bit address in rdi, pointing to the lock
    ; we just need to put the correct values at correct offsets and deal with interrupts for the processor
    ; rdi = lk->lock, rdi + 4 = lk->cpu_id, rdi + 8 = lk->rflags
    pushfq
    pop rdx
    mov qword [rdi + 8], rdx

    cli
    push rbx
.spin:
    mov eax, 1
    xchg dword [rdi], eax
    test eax, eax
    jnz .spin

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
    ; called the same as spinlock_acquire
    mov dword [rdi + 4], 0xffffffff ; lk->cpu_id = 0xffffffff;
    mov dword [rdi], 0x00000000 ; lk->lock = 0;

    push qword [rdi + 8]
    popfq
    ; restore rflags
    ret


spinlock_init:
    mov dword [rdi], 0 ; lk->lock = 0
    mov dword [rdi + 4], 0xffffffff
    ; lk->cpu_id = 0xffffffff
    mov qword [rdi + 8], 0
    ; lk->rflags = 0
    ret
