#include "include/pmap.h"
#include "include/mikroKernellib-common.h"
#include "include/phys_kmalloc.h"
#include <stdint.h>

extern uint16_t mmap_entry_count;
extern mmap_entry_0xe820 mmap_bios_entries[];

static char pmap_buf[PAGE_SIZE / 2] = {0};

void* memcpy(void* dest, const void* src, size_t n) {
    // Cast void pointers to unsigned char pointers for byte-level access
    unsigned char* d = (unsigned char*)dest;
    const unsigned char* s = (unsigned char*)src;

    // Copy byte by byte
    while (n--) {
        *d++ = *s++;
    }

    // Return the original destination pointer per standard convention
    return dest;
}

void __init_pmap() {
    memcpy(pmap_buf, mmap_bios_entries, mmap_entry_count * sizeof(mmap_entry_0xe820));
}

uint16_t get_pmap_entry_count() {
    return mmap_entry_count;
}

pmap_entry* get_pmap() {
    return (pmap_entry*)pmap_buf;
}
