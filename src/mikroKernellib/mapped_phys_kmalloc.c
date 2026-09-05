/* Eyal Kaghanovich
 * 30/07/26
 *
 * this is the kernel's equivalent of kmalloc
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

/* This layer hands out DIRECT-MAP pointers for callers that just want memory
 * they can use, as opposed to a physical frame they are going to put in a PTE.
 * It shares the single phys_map_offset from mmu.h rather than tracking its own
 * copy, so it can never drift out of step with directmap_p2v().
 *
 * Note the asymmetry with directmap_p2v(): that one translates at every
 * dereference and leaves stored values physical. This one bakes the offset into
 * the value it returns, so a pointer from here must NOT also be run through
 * directmap_p2v() - the guard in __directmap_p2v_raw() makes that harmless
 * rather than catastrophic, but it is still a sign the caller picked the wrong
 * allocator.
 */

/* TODO: rename to mapped_kmalloc */

static inline bool is_direct_mapped_ptr(const void* ptr) {
    return (uint64_t)ptr >= DIRECT_MAP_START && (uint64_t)ptr < DIRECT_MAP_END;
}

void* mapped_phys_kmalloc(const size_t size, const ALLOC_FLAG flg) {
    void* ptr = phys_kmalloc(size, flg);
    if (ptr == NULL) {
        return NULL;
    } else {
        return (void*)((uint64_t)ptr + phys_map_offset);
    }
}

bool mapped_phys_kfree(const void* ptr, const size_t size, const ALLOC_FLAG flg) {
    // check if ptr is in the direct map
    if (is_direct_mapped_ptr(ptr)) {
        // call free(ptr - direct map base addr)
        phys_kfree((void*)((uint64_t)ptr - phys_map_offset), size, flg);
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
        return (void*)((uint64_t)object + phys_map_offset);
    }
}

bool mapped_kmem_cache_kfree(kmem_cache* cache, const void* object) {
    if (is_direct_mapped_ptr(object)) {
        kmem_cache_kfree(cache, (void*)((uint64_t)object - phys_map_offset));
        return true;
    }
    return false;
}

void* mapped_buddy_kalloc(size_t size) {
    void* chunk = buddy_kalloc(size);
    if (chunk == NULL)
        return NULL;
    return (void*)((uint64_t)chunk + phys_map_offset);
}

bool mapped_buddy_kfree(void* chunk, size_t size) {
    // the direct-map test belongs on the pointer the caller passed in,
    // not on the physical address derived from it
    if (is_direct_mapped_ptr(chunk)) {
        buddy_kfree((void*)((uint64_t)chunk - phys_map_offset), size);
        return true;
    }
    return false;
}

static void zero_page(uint64_t* page) {
    for (uint64_t i = 0; i < PAGE_SIZE / sizeof(uint64_t); i++) {
        page[i] = 0;
    }
}

void* get_page() {
    void* page = get_zeroed_phys_page();
    if (page == NULL)
        return NULL;

    return (void*)((uint64_t)page + phys_map_offset);
}

void* get_zeroed_page() {
    void* page = get_page();
    if (page == NULL) return NULL;

    zero_page(page);
    return (void*)((uint64_t)page + phys_map_offset);
}

bool free_page(void* page) {
    if (is_direct_mapped_ptr(page)) {
        free_phys_page((void*)((uint64_t)page - phys_map_offset));
        return true;
    }
    return false;
}

// these freeing functions return bool if the ptr
// is not direct mapped, and then caller must check the bool,
// if it is false, the ptr is not direct mapped, and the appropriate
// physical deallocator needs to be called
