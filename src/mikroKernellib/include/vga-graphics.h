#pragma once

#include <stdint.h>

void vga_nt_printch(volatile const char* ch, uint32_t const row, uint32_t const col);
void vga_nt_println(volatile const char* str, const uint32_t row, uint32_t col);
void vga_clear_screen(void);
void vga_print_mmap(uint32_t row, uint32_t col);