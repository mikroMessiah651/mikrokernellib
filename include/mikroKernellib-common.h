#pragma once

#include "include/kasm.h"
#include "include/vesa_graphics_lib.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define PAGE_SIZE 0x1000ULL
#define HUGE_PAGE_SIZE 0x200000ULL

typedef int status_t;
#define STATUS_OK 0
#define STATUS_ERROR (-1)

#define container_of(ptr, type, member) \
    ((type*)((char*)(ptr) - offsetof(type, member)))

#define JMP goto
#define jmp goto

void* my_memcpy(void* dest, const void* src, size_t n);

#define PANIC(msg)                                               \
    {                                                            \
        vesa_nt_println("KERNEL PANIC: " msg, VESA_LAST_ROW, 0); \
        while (1) {                                              \
            hlt;                                                 \
        };                                                       \
    }
