#include <stdint.h>
#include "include/vga-graphics.h"
#include "include/mikroKernellib-common.h"

#define video_buffer (volatile uint16_t*)0xB8000


void vga_nt_printch(volatile const char* ch, uint32_t const row, uint32_t const col) {
    volatile uint16_t* vga = video_buffer;
    vga[row * 80 + col] = (uint16_t)(((uint16_t)*ch) | (0x0F << 8));
}


void vga_nt_println(volatile const char* str, const uint32_t row, uint32_t col) {
    while (str[0] != '\0') {
        vga_nt_printch(&str[0], row, col);
        col++;
        str++;
    }
}

typedef struct {
    void* base_address;
    uint64_t chunk_size;
    uint32_t type;
    uint32_t ACPI_ext_attr;
} __attribute__((packed)) mmap_entry_0xe820;

extern uint16_t mmap_entry_count;
extern mmap_entry_0xe820 mmap_bios_entries[];

static void uint64_to_hex(uint64_t val, char* buf) {
    static const char hex[] = "0123456789ABCDEF";
    buf[0] = '0'; buf[1] = 'x';
    for (int i = 0; i < 16; i++)
        buf[2 + i] = hex[(val >> (60 - i * 4)) & 0xF];
    buf[18] = '\0';
}

static void uint32_to_dec(uint32_t val, char* buf) {
    /* prints up to 10 decimal digits, right-aligned in a fixed 10-char field */
    static const char dec[] = "0123456789";
    buf[10] = '\0';
    for (int i = 9; i >= 0; i--) {
        buf[i] = dec[val % 10];
        val /= 10;
    }
}

static const char* mmap_type_str(uint32_t type) {
    switch (type) {
        case 1: return "Usable RAM  ";
        case 2: return "Reserved    ";
        case 3: return "ACPI Reclaim";
        case 4: return "ACPI NVS    ";
        case 5: return "Bad Memory  ";
        default: return "Unknown     ";
    }
}

void vga_print_mmap(uint32_t row, const uint32_t col) {
    char hex_buf[19];
    char dec_buf[11];
    for (int i = 0; i < mmap_entry_count; i++) {
        uint32_t c = col;

        uint32_to_dec((uint32_t)i, dec_buf);
        vga_nt_println(dec_buf + 9, row, c); /* print last digit(s) */
        c += 2;

        vga_nt_println("Base:", row, c); c += 5;
        uint64_to_hex((uint64_t)mmap_bios_entries[i].base_address, hex_buf);
        vga_nt_println(hex_buf, row, c); c += 19;

        vga_nt_println("Size:", row, c); c += 5;
        uint64_to_hex(mmap_bios_entries[i].chunk_size, hex_buf);
        vga_nt_println(hex_buf, row, c); c += 19;

        vga_nt_println("Type:", row, c); c += 5;
        vga_nt_println(mmap_type_str(mmap_bios_entries[i].type), row, c);

        row++;
    }
}


void vga_clear_screen(void) {
    volatile uint16_t* vga = video_buffer;
    for (int i = 0; i < 80 * 25; i++) {
        vga[i] = 0x0000;
    }
}
