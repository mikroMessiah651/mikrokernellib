[bits 64]

global pml5_detect
global load_pmlt_cr3
global enter_virtual

section .text

pml5_detect:
    mov eax, 7          ; CPUID leaf 7 (extended features)
    xor ecx, ecx        ; subleaf 0
    
    cpuid

    bt ecx, 16          ; (puts 16th bit of ecx in the carry flag)
    jc supports_pml5    ; if cf set -> LA57 support -> pml5t supported

    mov rax, 0 ; return 0
    ret

supports_pml5:
    mov rax, 1 ; return 1
    ret

    
load_pmlt_cr3:
    mov cr3, rdi
    ret

;enter_virtual(virt_rsp, virt_entry);
enter_virtual:
    mov rsp, rdi
    xor rbp, rbp
    jmp rsi

