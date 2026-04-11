#pragma once


#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "include/kasm.h"
#include "include/vesa_graphics_lib.h"

#define FALSE 0
#define TRUE 1

#define PAGE_SIZE 0x1000ULL
#define HUGE_PAGE_SIZE 0x200000ULL

typedef int status_t;
#define STATUS_OK 0
#define STATUS_ERROR (-1)

#define PANIC(msg) { vesa_nt_println("KERNEL PANIC: " msg, VESA_LAST_ROW, 0); while(1) {hlt;}; }