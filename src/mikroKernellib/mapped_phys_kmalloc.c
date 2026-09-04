/* Eyal Kaghanovich
 * 30/07/26
 *
 * this is the kernel's equivalant of kmalloc
 * it returns a physically and virtually contiguous, direct mapped
 * chunk of memory of a given size
 */


#include "include/mapped_phys_kmalloc.h"
#include "include/mikroKernellib-common.h"
#include "include/mmu.h"
#include "include/phys_kmalloc.h"
#include "include/vm.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

static offset_t direct_map_offset = 0; // changes after vm init when the direct nmap is on

static inline bool is_direct_mapped_ptr(const void* ptr) {
    if ((void*)direct_map_offset <= ptr && ptr < (void*)DIRECT_MAP_END) {
        return true;
    } else {
        return false;
    }
}

void _init_dm_pmm() {
    direct_map_offset = DIRECT_MAP_START;
}

void* mapped_phys_kmalloc(const size_t size, const ALLOC_FLAG flg) {
    void* ptr = phys_kmalloc(size, flg);
    if (ptr == NULL) {
        return NULL;
    } else {
        return (void*)((uint64_t)ptr + direct_map_offset);
    }
}

bool mapped_phys_kfree(const void* ptr, const size_t size, const ALLOC_FLAG flg) {
    // check if ptr is in the direct map
    if (is_direct_mapped_ptr(ptr)) {
        // call free(ptr - direct map base addr)
        phys_kfree((void*)(uint64_t)(ptr - direct_map_offset), size, flg);
        return true;
    }
    return false;
}

// for direct-mapped specific kmem_cache allocations use this functions:
void* mapped_kmem_cache_kalloc(kmem_cache* cache) {
    void* object = kmem_cache_kalloc(cache);
    if (object == NULL) {
        return NULL;
    } else {
        return (void*)((uint64_t)(object + direct_map_offset));
    }
}

bool mapped_kmem_cache_kfree(kmem_cache* cache, const void* object) {
    void* phys_object = (void*)((uint64_t)(object - direct_map_offset));

    if (is_direct_mapped_ptr(object)) {
        kmem_cache_kfree(cache, phys_object);
        return true;
    }
    return false;
}

void* mapped_buddy_kalloc(size_t size) {
    void* chunk = buddy_kalloc(size);
    if (chunk == NULL)
        return NULL;
    return (void*)((uint64_t)chunk + direct_map_offset);
}

bool mapped_buddy_kfree(void* chunk, size_t size) {
    void* phys_chunk = (void*)((uint64_t)chunk - direct_map_offset);

    if (is_direct_mapped_ptr(phys_chunk)) {
        buddy_kfree(phys_chunk, size);
        return true;
    }
    return false;
}

void* get_page() {
    void* page = get_zeroed_phys_page();
    if (page == NULL)
        return NULL;

    return (void*)((uint64_t)page + direct_map_offset);
}

bool free_page(void* page) {
    void* phys_page = (void*)(uint64_t)page - direct_map_offset;
    if (is_direct_mapped_ptr(phys_page)) {
        free_phys_page(phys_page);
        return true;
    }
    return false;
}

// these freeing functions return bool if the ptr
// is not direct mapped, and then caller must check the bool,
// if it is false, the ptr is not direct mapped, and the appropriate
// physical deallcoator needs to be called
