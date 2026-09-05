#pragma once

#include "include/phys_kmalloc.h"
#include <stddef.h>

void* mapped_phys_kmalloc(size_t size, ALLOC_FLAG flg);
bool mapped_phys_kfree(const void* ptr, size_t size, ALLOC_FLAG flg);

void* mapped_buddy_kalloc(size_t size);
bool mapped_buddy_kfree(void* chunk, size_t size);

void* get_page();
bool free_page(void* page);

// for specific cache allocations
void* mapped_kmem_cache_kalloc(kmem_cache* cache);
bool mapped_kmem_cache_kfree(kmem_cache* cache, const void* object);
