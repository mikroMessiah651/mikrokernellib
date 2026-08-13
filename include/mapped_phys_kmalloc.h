#pragma once

#include "include/phys_kmalloc.h"

void* mapped_phys_kmalloc(size_t size, ALLOC_FLAG flg);
bool mapped_phys_kfree(const void* ptr, size_t size, ALLOC_FLAG flg);

// for specific cache allocations
void* mapped_kmem_cache_kalloc(kmem_cache* cache);
bool mapped_kmem_cache_kfree(kmem_cache* cache, const void* object);
