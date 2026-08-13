// Created by eyalka on 3/28/26.

#include "include/vesa_graphics_lib.h"

extern uint8_t font_data[];
extern uint8_t font_data_end[];

typedef struct {
    uint8_t magic[2];
    uint8_t mode;
    uint8_t charsize;
} __attribute__((packed)) psf1_hdr;

typedef struct {
    uint8_t magic[4]; // 0x72 0xB5 0x4A 0x86
    uint32_t version;
    uint32_t headersize; // usually 32
    uint32_t flags;
    uint32_t numglyph;
    uint32_t bytesperglyph;
    uint32_t height;
    uint32_t width;
} __attribute__((packed)) psf2_hdr;

#define PSF2_MAGIC0 0x72
#define PSF2_MAGIC1 0xB5

static inline int is_psf2(void) {
    return font_data[0] == PSF2_MAGIC0 && font_data[1] == PSF2_MAGIC1;
}

static inline uint32_t psf_charheight(void) {
    if (is_psf2())
        return ((const psf2_hdr*)font_data)->height;
    return ((const psf1_hdr*)font_data)->charsize;
}

static inline const uint8_t* psf_glyph(const uint8_t c) {
    if (is_psf2()) {
        const psf2_hdr* hdr = (const psf2_hdr*)font_data;
        return font_data + hdr->headersize + c * hdr->bytesperglyph;
    }
    const psf1_hdr* hdr = (const psf1_hdr*)font_data;
    return font_data + sizeof(psf1_hdr) + c * hdr->charsize;
}

typedef struct {
    void* base_address;
    uint64_t chunk_size;
    uint32_t type;
    uint32_t acpi_ext_attr;
} __attribute__((packed)) mmap_entry_t;

extern uint16_t mmap_entry_count;
extern mmap_entry_t mmap_bios_entries[];

static uint64_t s_fb_virt = 0;

void vesa_set_fb_virtual(uint64_t virtual_fb_base) {
    s_fb_virt = virtual_fb_base;
}

static inline volatile uint32_t* get_fb(const boot_vbe_handoff* vbe_md) {
    if (s_fb_virt)
        return (volatile uint32_t*)s_fb_virt;
    return (volatile uint32_t*)(uintptr_t)vbe_md->framebuffer_address;
}

static void uint64_to_hex(uint64_t val, char* buf) {
    static const char hex[] = "0123456789ABCDEF";
    buf[0] = '0';
    buf[1] = 'x';
    for (int i = 0; i < 16; i++)
        buf[2 + i] = hex[(val >> (60 - i * 4)) & 0xF];
    buf[18] = '\0';
}

static const char* mmap_type_str(uint32_t type) {
    switch (type) {
    case 1:
        return "Usable RAM  ";
    case 2:
        return "Reserved    ";
    case 3:
        return "ACPI Reclaim";
    case 4:
        return "ACPI NVS    ";
    case 5:
        return "Bad Memory  ";
    default:
        return "Unknown     ";
    }
}

void vbe_blue_screen(void) {
    const boot_vbe_handoff* vbe_md =
        (const boot_vbe_handoff*)vbe_handoff_address;
    volatile uint32_t* fb = get_fb(vbe_md);
    const uint32_t pitch_in_pixels = vbe_md->pitch / 4;
    for (uint32_t y = 0; y < vbe_md->height_px; y++) {
        for (uint32_t x = 0; x < vbe_md->width_px; x++) {
            fb[y * pitch_in_pixels + x] = 0x00ADD8E6;
        }
    }
}

void vbe_black_screen(void) {
    const boot_vbe_handoff* vbe_md =
        (const boot_vbe_handoff*)vbe_handoff_address;
    volatile uint32_t* fb = get_fb(vbe_md);
    const uint32_t pitch_in_pixels = vbe_md->pitch / 4;
    for (uint32_t y = 0; y < vbe_md->height_px; y++) {
        for (uint32_t x = 0; x < vbe_md->width_px; x++) {
            fb[y * pitch_in_pixels + x] = 0x00000000;
        }
    }
}

void vesa_clear_lower_half(void) {
    const boot_vbe_handoff* vbe_md =
        (const boot_vbe_handoff*)vbe_handoff_address;
    volatile uint32_t* fb = get_fb(vbe_md);
    const uint32_t pitch_in_pixels = vbe_md->pitch / 4;
    const uint32_t half_row_px =
        (uint32_t)(VESA_LAST_ROW / 2 + 1) * psf_charheight();
    for (uint32_t y = half_row_px; y < vbe_md->height_px; y++) {
        for (uint32_t x = 0; x < vbe_md->width_px; x++) {
            fb[y * pitch_in_pixels + x] = 0x00000000;
        }
    }
}

void vesa_print_virt_addr(const uint64_t addr, const int row, const int col) {
    static const char hex[] = "0123456789ABCDEF";
    char buf[19]; // "0x" + 16 nibbles + '\0'
    buf[0] = '0';
    buf[1] = 'x';
    for (int i = 0; i < 16; i++)
        buf[2 + i] = hex[(addr >> (60 - i * 4)) & 0xF];
    buf[18] = '\0';
    vesa_nt_println(buf, row, col);
}

void vesa_print_mmap(int row, const int col) {
    char hex_buf[19];
    for (int i = 0; i < mmap_entry_count; i++) {
        int c = col;

        char idx_buf[3] = {'0' + (i / 10), '0' + (i % 10), '\0'};
        vesa_nt_println(idx_buf, row, c);
        c += 3;

        vesa_nt_println("Base:", row, c);
        c += 5;
        uint64_to_hex((uint64_t)mmap_bios_entries[i].base_address, hex_buf);
        vesa_nt_println(hex_buf, row, c);
        c += 19;

        vesa_nt_println("Size:", row, c);
        c += 5;
        uint64_to_hex(mmap_bios_entries[i].chunk_size, hex_buf);
        vesa_nt_println(hex_buf, row, c);
        c += 19;

        vesa_nt_println("Type:", row, c);
        c += 5;
        vesa_nt_println(mmap_type_str(mmap_bios_entries[i].type), row, c);

        row++;
    }
}

// 1280x720 with 8x16 font gives 160 columns (1280/8) and 45 rows (720/16)
void vesa_nt_println(const char* nt_s, const int row, const int col) {
    const boot_vbe_handoff* vbe_md =
        (const boot_vbe_handoff*)vbe_handoff_address;
    volatile uint32_t* fb = get_fb(vbe_md);
    const uint32_t pitch_in_pixels = vbe_md->pitch / 4;
    const uint32_t charheight = psf_charheight();

    int cur_col = col;
    for (int i = 0; nt_s[i] != '\0'; i++, cur_col++) {
        const uint8_t* glyph = psf_glyph((uint8_t)nt_s[i]);
        const uint32_t px_x = (uint32_t)cur_col * 8;
        const uint32_t px_y = (uint32_t)row * charheight;

        for (uint32_t r = 0; r < charheight; r++) {
            for (uint8_t c = 0; c < 8; c++) {
                const uint32_t color =
                    (glyph[r] & (0x80 >> c)) ? 0x00FFFFFF : 0x00000000;
                fb[(px_y + r) * pitch_in_pixels + (px_x + c)] = color;
            }
        }
    }
}