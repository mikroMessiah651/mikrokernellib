//
// Created by eyalka on 3/28/26.
//

#pragma once
#include <stdint.h>

#define VESA_LAST_ROW 83
#define VESA_LAST_COL 318

/* seg_offset_to_lm(seg, off) — real-mode seg:off pair → 64-bit physical address
 * Use for actual seg:off pairs (e.g. WinFuncPtr). The VBE PhysBasePtr
 * (framebuffer_address) is already a flat physical address — cast it directly:
 *   volatile uint32_t* fb = (volatile
 * uint32_t*)(uintptr_t)vbe_md->framebuffer_address; */
#define seg_offset_to_lm(seg, off) ((uintptr_t)((seg) << 4) + (uintptr_t)(off))

#define vbe_handoff_address 0x7100

typedef struct {
    uint32_t framebuffer_address;
    uint16_t pitch;
    uint16_t width_px;
    uint16_t height_px;
    uint8_t bpp;
} __attribute__((packed)) boot_vbe_handoff;

void vesa_set_fb_virtual(uint64_t virtual_fb_base);
void vbe_blue_screen(void);
void vbe_black_screen(void);
void vesa_print_mmap(int row, int col);
void vesa_clear_lower_half(void);
void vesa_print_virt_addr(uint64_t addr, int row, int col);
// row: 0–44, col: 0–159  (1280x720 / 8x16 font)
void vesa_nt_println(const char* nt_s, int row, int col);