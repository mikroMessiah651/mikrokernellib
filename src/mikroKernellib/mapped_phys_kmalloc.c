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
#include <stdint.h>


static inline bool is_direct_mapped_ptr(const void* ptr) {
    if ((void*)DIRECT_MAP_START <= ptr && ptr < (void*)DIRECT_MAP_END) {
        return true;
    } else {
        return false;
    }
}

void* mapped_phys_kmalloc(const size_t size, const ALLOC_FLAG flg) {
    void* ptr = phys_kmalloc(size, flg);
    if (ptr == NULL) {
        return NULL;
    } else {
        return (void*)((uint64_t)ptr + DIRECT_MAP_START);
    }
}

bool mapped_phys_kfree(const void* ptr, const size_t size, const ALLOC_FLAG flg) {
    // check if ptr is in the direct map
    if (is_direct_mapped_ptr(ptr)) {
        // call free(ptr - direct map base addr)
        phys_kfree((void*)(uint64_t)(ptr - DIRECT_MAP_START), size, flg);
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
        return (void*)((uint64_t)(object + DIRECT_MAP_START));
    }
}

bool mapped_kmem_cache_kfree(kmem_cache* cache, const void* object) {
    void* phys_object = (void*)((uint64_t)(object - DIRECT_MAP_START));

    if (is_direct_mapped_ptr(object)) {
        kmem_cache_kfree(cache, phys_object);
        return true;
    }
    return false;
}

// if memory is not direct mapped, should it still be freed?
// these freeing functions return bool if the ptr
// is not direct mapped, and then caller must check the bool,
// if it is false, the ptr is not direct mapped, and the regular
// phys_kfree() needs to be called
