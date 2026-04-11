[bits 64]

global pml5_detect
global load_pmlt_cr3
global jump_to_virt_and_switch_stack

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


; rdi = new virtual rsp, rsi = virt_offset (KERNEL_IMAGE_START - KERNEL_PHYS_BASE)                                        
jump_to_virt_and_switch_stack:                                                                                            
    pop rax              ; save return address (physical addr in __init_virtual_memory)                                   
    add rax, rsi         ; convert to virtual                                                                             
    mov rsp, rdi         ; switch to virtual stack                                                                        
    push rax             ; push virtual return addr onto virtual stack                                                    
    lea rax, [rel .target]                                                                                                
    add rax, rsi                                                                                                          
    jmp rax                                                                                                               

.target:                                                                                                                  
    ret                  
    ; pops virtual addr → continues in __init_virtual_memory at virtual RIP                          
    ; but __init_virtual_memory is noreturn, so that's the end