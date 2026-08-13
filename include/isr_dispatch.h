#pragma once

#include <stdint.h>

typedef struct {
    uint64_t r15, r14, r13, r12, r11, r10, r9, r8, rbp, rdi, rsi, rdx, rcx, rbx,
        rax;
    uint64_t vec, error_code;
    uint64_t rip, cs, rflags, rsp, ss;
} registers_t;

typedef struct {
    uint64_t error_code;
    void* vaddr;
} pf_info_t;