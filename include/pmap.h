#pragma once


// API for getting the e820 memory map
// via a valid virtual address in the direct map
// TODO: potentially anyone can write to the pmap.
// make it readonly with page table manipulation?

#include "include/phys_kmalloc.h"
#include <stdint.h>

void __init_pmap();
typedef mmap_entry_0xe820 pmap_entry;

pmap_entry* get_pmap();

uint16_t get_pmap_entry_count();
