
[bits 64]

section .text

global mov_rax_cr2
global pf_debug
global idt_load
global isr0, isr1, isr2, isr3, isr4, isr5, isr6, isr7
global isr8, isr9, isr10, isr11, isr12, isr13, isr14, isr15
global isr16, isr17, isr18, isr19, isr20, isr21, isr22, isr23
global isr24, isr25, isr26, isr27, isr28, isr29, isr30, isr31

extern isr_dispatch

idt_load:
    lidt [rdi]
    ret

mov_rax_cr2:
    mov rax, cr2
    ret

isr_common_handler:
    push rax
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi
    push rbp
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15

    mov rdi, rsp ; C treats rdi as the argument to isr_dispatch (registers_t* rdi)
    ; so we pass the ptr to the struct we pushed on the stack
    call isr_dispatch

    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    pop rbp
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    pop rax

    add rsp, 16
    iretq

; No CPU error code — push dummy 0, then vector
isr0:               ; #DE  Divide Error

    push 0x00
    push 0x00
    jmp isr_common_handler

isr1:               ; #DB  Debug

    push 0x00
    push 0x01
    jmp isr_common_handler

isr2:               ; NMI  Non-Maskable Interrupt

    push 0x00
    push 0x02
    jmp isr_common_handler

isr3:               ; #BP  Breakpoint

    push 0x00
    push 0x03
    jmp isr_common_handler

isr4:               ; #OF  Overflow

    push 0x00
    push 0x04
    jmp isr_common_handler

isr5:               ; #BR  BOUND Range Exceeded

    push 0x00
    push 0x05
    jmp isr_common_handler

isr6:               ; #UD  Invalid Opcode

    push 0x00
    push 0x06
    jmp isr_common_handler

isr7:               ; #NM  Device Not Available

    push 0x00
    push 0x07
    jmp isr_common_handler


; CPU pushes error code — push vector only
isr8:               ; #DF  Double Fault

    push 0x08
    jmp isr_common_handler


; No CPU error code
isr9:               ; Coprocessor Segment Overrun (reserved)

    push 0x00
    push 0x09
    jmp isr_common_handler


; CPU pushes error code — push vector only
isr10:              ; #TS  Invalid TSS

    push 0x0A
    jmp isr_common_handler

isr11:              ; #NP  Segment Not Present

    push 0x0B
    jmp isr_common_handler

isr12:              ; #SS  Stack-Segment Fault

    push 0x0C
    jmp isr_common_handler

isr13:              ; #GP  General Protection

    push 0x0D
    jmp isr_common_handler

isr14:              ; #PF  Page Fault

    push 0x0E
    jmp isr_common_handler

; No CPU error code
isr15:              ; Reserved

    push 0x00
    push 0x0F
    jmp isr_common_handler

isr16:              ; #MF  x87 FPU Error

    push 0x00
    push 0x10
    jmp isr_common_handler


; CPU pushes error code
isr17:              ; #AC  Alignment Check

    push 0x11
    jmp isr_common_handler


; No CPU error code
isr18:              ; #MC  Machine Check

    push 0x00
    push 0x12
    jmp isr_common_handler

isr19:              ; #XM  SIMD Floating-Point Exception

    push 0x00
    push 0x13
    jmp isr_common_handler

isr20:              ; #VE  Virtualization Exception

    push 0x00
    push 0x14
    jmp isr_common_handler


; CPU pushes error code
isr21:              ; #CP  Control Protection

    push 0x15
    jmp isr_common_handler


; No CPU error code
isr22:              ; Reserved

    push 0x00
    push 0x16
    jmp isr_common_handler

isr23:              ; Reserved

    push 0x00
    push 0x17
    jmp isr_common_handler

isr24:              ; Reserved

    push 0x00
    push 0x18
    jmp isr_common_handler

isr25:              ; Reserved

    push 0x00
    push 0x19
    jmp isr_common_handler

isr26:              ; Reserved

    push 0x00
    push 0x1A
    jmp isr_common_handler

isr27:              ; Reserved

    push 0x00
    push 0x1B
    jmp isr_common_handler

isr28:              ; #HV  Hypervisor Injection Exception

    push 0x00
    push 0x1C
    jmp isr_common_handler


; CPU pushes error code
isr29:              ; #VC  VMM Communication Exception

    push 0x1D
    jmp isr_common_handler

isr30:              ; #SX  Security Exception

    push 0x1E
    jmp isr_common_handler


; No CPU error code
isr31:              ; Reserved

    push 0x00
    push 0x1F
    jmp isr_common_handler
