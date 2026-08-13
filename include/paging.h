#pragma once

#include <stddef.h>
#include <stdint.h>

void kmap_page(const void* paddr, const void* vaddr, const uint64_t flags);

void kunmap_page(const void* paddr, const void* vaddr);

void kmap_huge_page(const void* paddr, const void* vaddr, const uint64_t flags);

void kunmap_huge_page(const void* paddr, const void* vaddr);

void kmap_pages(const void* paddr, const void* vaddr, const uint64_t flags,
                const size_t num_pages);

void kunmap_pages(const void* paddr, const void* vaddr, const uint64_t flags,
                  const uint64_t num_pages);
