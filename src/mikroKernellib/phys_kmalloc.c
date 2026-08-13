/*
 * author: Eyal Kaghanovich
 * PMM for the kernel, buddy and slab implementations
 * needs heavy cleaning
 */

#include "include/phys_kmalloc.h"
#include "include/kmath.h"
#include "include/mikroKernellib-common.h"
#include "include/spinlocks.h"
#include "include/vesa_graphics_lib.h"

#define NUM_MAX_ORDER (metadata->max_order + 1) // 11

extern uint64_t _kernel_end;
uint64_t kernel_end = (uint64_t)&_kernel_end;

static spinlock_t buddy_lock;
static spinlock_t slab_lock;

static inline void memset_lb(void* ptr, const uint8_t val,
                             const uint64_t size) {
    uint8_t* p = (uint8_t*)ptr;
    for (uint64_t i = 0; i < size; i++) {
        p[i] = val;
    }
}

typedef struct buddy_chunk {
    struct buddy_chunk* next;
    struct buddy_chunk* prev;
} buddy_chunk;

typedef struct {
    void* base_address;
    uint64_t size;
    uint8_t min_order;
    uint8_t max_order;
    buddy_chunk** free_lists_ptr;
    void* bitmap_ptr;
    uint32_t order_offsets[11]; // order_offsets[k] = number of bitmap bits
                                // belonging to orders below k
} __attribute__((packed)) buddy_metadata;

extern uint16_t mmap_entry_count;
extern mmap_entry_0xe820 mmap_bios_entries[];

void __init_buddy() {
    // needs heavy clean-up...
    spinlock_init(&buddy_lock);

    vesa_print_mmap(17, 0);
    buddy_metadata* metadata = (buddy_metadata*)kernel_end;
    metadata->min_order = 0;
    metadata->max_order = 10;
    metadata->free_lists_ptr = (buddy_chunk**)((buddy_metadata*)kernel_end + 1);
    memset_lb(metadata->free_lists_ptr, 0x00,
              NUM_MAX_ORDER * sizeof(buddy_chunk*));

    void* best_base_addr = 0;
    uint64_t best_chunk_size = 0;
    // now we need to walk over e820
    for (int idx = 0; idx < mmap_entry_count; idx++) {
        if (mmap_bios_entries[idx].type != 0x01)
            continue;

        void* coalescence_base = mmap_bios_entries[idx].base_address;
        uint64_t coalescence_size = mmap_bios_entries[idx].chunk_size;

        // coalesce contiguous type 0x01 neighbours
        while (idx + 1 < mmap_entry_count &&
               mmap_bios_entries[idx + 1].type == 0x01 &&
               (uint64_t)mmap_bios_entries[idx + 1].base_address ==
                   (uint64_t)coalescence_base + coalescence_size) {
            coalescence_size += mmap_bios_entries[++idx].chunk_size;
        }

        if (coalescence_size > best_chunk_size) {
            best_chunk_size = coalescence_size;
            best_base_addr = coalescence_base;
        }
    }

    const uint64_t worst_case_bitmap = (best_chunk_size / PAGE_SIZE) / 8;
    // best_base_addr must clear: 1MB low memory, kernel image, and metadata
    // sitting after kernel
    uint64_t lower_bound = ONE_MB_ADDRESS;
    const uint64_t after_metadata = kernel_end + sizeof(buddy_metadata) +
                                    NUM_MAX_ORDER * sizeof(buddy_chunk*) +
                                    worst_case_bitmap;

    if (after_metadata > lower_bound)
        lower_bound = after_metadata;
    // if kernel binary + metadata > 1MB => allocate memory from after kernel +
    // metadata

    if ((uint64_t)best_base_addr < lower_bound) {
        if (lower_bound < (uint64_t)best_base_addr + best_chunk_size) {
            best_chunk_size -= lower_bound - (uint64_t)best_base_addr;
            best_base_addr = (void*)lower_bound;
        }
    }

    // align best_base_addr to 4MB, largest order allocation
    // important for xor buddy addressing magic
    const uint64_t base = (uint64_t)best_base_addr;
    const uint64_t max_order_align = (1ULL << metadata->max_order) * PAGE_SIZE;
    const uint64_t align = (max_order_align - (base & (max_order_align - 1))) &
                           (max_order_align - 1);
    best_base_addr = (void*)(base + align);
    best_chunk_size -= align;
    best_chunk_size = (best_chunk_size) & ~(PAGE_SIZE - 1);
    // this reduces the size of the memory the buddy manages
    // but, it makes sure it's divisible by PAGE_SIZE

    metadata->base_address = best_base_addr;
    metadata->size = best_chunk_size;

    // we now need free lists array and bitmap...
    // we need to split our contiguous chunk into smaller power of two chunks
    // and insert them to free lists
    uint32_t counts[NUM_MAX_ORDER];
    for (uint8_t i = 0; i < NUM_MAX_ORDER; i++)
        counts[i] = 0;

    uint64_t addr = (uint64_t)metadata->base_address;
    uint64_t remaining = metadata->size;

    while (remaining >= PAGE_SIZE) {
        // find the largest order whose chunk fits and whose size divides addr
        int order = metadata->max_order;
        while (order >= metadata->min_order) {
            const uint64_t chunk_bytes = (1ULL << order) * PAGE_SIZE;
            if (chunk_bytes <= remaining && (addr & (chunk_bytes - 1)) == 0)
                break;
            order--;
        }
        if (order < metadata->min_order)
            break;

        const uint64_t chunk_bytes = (1ULL << order) * PAGE_SIZE;
        counts[order]++;
        addr += chunk_bytes;
        remaining -= chunk_bytes;
    }

    buddy_chunk* current_addr = best_base_addr;
    for (int order = metadata->max_order; order >= 0; order--) {
        buddy_chunk* head = current_addr;
        const uint32_t count = counts[order];

        if (count == 0)
            continue;

        for (uint32_t i = 0; i < count; i++) {
            buddy_chunk* next_chunk =
                (struct buddy_chunk*)((uint8_t*)current_addr +
                                      (1ULL << order) * PAGE_SIZE);
            current_addr->next = i == count - 1 ? NULL : next_chunk;
            current_addr->prev =
                i == 0 ? NULL
                       : (buddy_chunk*)((uint8_t*)current_addr -
                                        (1ULL << order) * PAGE_SIZE);
            current_addr = next_chunk;
        }

        metadata->free_lists_ptr[order] = head;
    }

    const uint64_t num_pages = metadata->size / PAGE_SIZE;
    const uint64_t bitmap_size_bytes = num_pages / 8;

    metadata->bitmap_ptr = (void*)(kernel_end + sizeof(buddy_metadata) +
                                   NUM_MAX_ORDER * sizeof(buddy_chunk*));
    // puts bitmap after everything it needs to be
    // now we need to memset the bitmap
    memset_lb(metadata->bitmap_ptr, 0x00, bitmap_size_bytes);

    // precompute order_offsets: order_offsets[k] = total bits used by orders
    // 0..k-1
    metadata->order_offsets[0] = 0;
    for (int k = 1; k < NUM_MAX_ORDER; k++) {
        const uint32_t bits_at_prev =
            num_pages >> k; // buddy pairs at order k-1
        metadata->order_offsets[k] =
            metadata->order_offsets[k - 1] + bits_at_prev;
    }

    vesa_nt_println("Initialized buddy allocation...\0", 1, 0);
}

// Toggle the XOR bit for a buddy pair. Returns the new bit value.
// Bit = 0 means both buddies are in the same state (both free or both
// allocated). Bit = 1 means they differ.
static inline uint8_t toggle_bitmap(const buddy_metadata* metadata,
                                    const uint64_t addr, const uint8_t order) {
    uint8_t* bitmap = (uint8_t*)metadata->bitmap_ptr;
    const uint32_t block_idx =
        (addr - (uint64_t)metadata->base_address) >> (12 + order);
    const uint32_t pair_idx = block_idx >> 1;
    const uint32_t bit_pos = metadata->order_offsets[order] + pair_idx;
    const uint32_t byte_idx = bit_pos / 8;
    const uint8_t bit_offset = bit_pos % 8;
    spinlock_acquire(&pmm_lock);
    bitmap[byte_idx] ^= (1 << bit_offset);
    const uint8_t ret = (bitmap[byte_idx] >> bit_offset) & 1;
    spinlock_release(&pmm_lock);
    return ret;
}

void* buddy_kalloc(const size_t size) {
    const buddy_metadata* metadata = (buddy_metadata*)kernel_end;
    uint8_t order = 0;
    while (size > (1ULL << order) * PAGE_SIZE)
        order++;
    if (order > metadata->max_order)
        return NULL; // if requested size is too big return null

    for (int idx = order; idx < NUM_MAX_ORDER; idx++) {
        spinlock_acquire(&pmm_lock);
        if (metadata->free_lists_ptr[idx] == NULL) {
            spinlock_release(&pmm_lock);
            continue;
        }
        // pop block from free list
        buddy_chunk* chunk = metadata->free_lists_ptr[idx];
        metadata->free_lists_ptr[idx] = chunk->next;
        if (chunk->next != NULL)
            chunk->next->prev = NULL;
        chunk->next = NULL;

        // split
        for (int level = idx; level > order; level--) {
            buddy_chunk* buddy =
                (buddy_chunk*)((uint8_t*)chunk +
                               (1ULL << (level - 1)) *
                                   PAGE_SIZE); // buddy after split
            buddy->prev = NULL;                // will be at head so prev must be null
            buddy->next =
                metadata
                    ->free_lists_ptr[level - 1]; // since we are inserting at
                                                 // head of the lower list, next
                                                 // must be the current head
            if (buddy->next != NULL)
                buddy->next->prev = buddy; // if it's the only chunk in the list
                                           // of current order
            metadata->free_lists_ptr[level - 1] =
                buddy; // free lists ptr points to buddy
        }
        spinlock_release(&pmm_lock);

        toggle_bitmap(metadata, (uint64_t)chunk, order);
        return (void*)chunk;
    }

    return NULL;
}

void buddy_kfree(void* chunk, const size_t size) {
    const buddy_metadata* metadata = (buddy_metadata*)kernel_end;

    uint8_t order = 0;
    while (size > (1ULL << order) * PAGE_SIZE)
        order++;
    if (order > metadata->max_order)
        order = metadata->max_order;
    // toggle bitmap at final order (needed when order == max_order, since the
    // loop doesn't run)
    if (order == metadata->max_order)
        toggle_bitmap(metadata, (uint64_t)chunk, order);

    while (order < metadata->max_order) {
        // toggle the xor bit for this buddy
        // bit=0 after toggle -> both buddies free -> coalesce
        // bit=1 after toggle -> buddies differ -> stop
        const uint8_t bit = toggle_bitmap(metadata, (uint64_t)chunk, order);
        if (bit != 0)
            break;

        // buddy address via xor, base must be aligned to max order
        buddy_chunk* buddy =
            (buddy_chunk*)((uint64_t)chunk ^ ((1ULL << order) * PAGE_SIZE));

        // remove buddy from free list
        spinlock_acquire(&pmm_lock);
        if (buddy->prev != NULL)
            buddy->prev->next = buddy->next;
        else
            metadata->free_lists_ptr[order] = buddy->next;
        if (buddy->next != NULL)
            buddy->next->prev = buddy->prev;
        buddy->next = NULL;
        buddy->prev = NULL;

        // coalesced block is at the lower address
        if ((uint64_t)buddy < (uint64_t)chunk)
            chunk = (void*)buddy;
        spinlock_release(&pmm_lock);

        order++;
    }

    spinlock_acquire(&pmm_lock);
    // insert coalesced block into free list at final order
    buddy_chunk* block = chunk;
    block->prev = NULL;
    block->next = metadata->free_lists_ptr[order];
    if (block->next != NULL)
        block->next->prev = block;
    metadata->free_lists_ptr[order] = block;
    spinlock_release(&pmm_lock);
}

/* TODO decide which slabs go back to buddy_alloc() */


#define NUM_CACHES 8
static kmem_cache slab_caches[NUM_CACHES];

static uint64_t calc_alignment(kmem_cache* cache) {
    const uint64_t first_object_offset =
        (sizeof(slab_descriptor) + cache->object_align - 1) &
        ~(cache->object_align - 1);
    const uint64_t alignment = first_object_offset - sizeof(slab_descriptor);
    return alignment;
}

static uint64_t calc_objects_per_slab(kmem_cache* cache) {
    // available memory = slab size - sizeof(descriptor) - padding,
    // padding is added such that the number of bytes left is divisible by
    // object_size
    const uint64_t alignment = calc_alignment(cache);

    const int total_bytes = PAGE_SIZE << cache->slab_order;
    const uint64_t rem = total_bytes - sizeof(slab_descriptor) - alignment;

    const uint64_t objects_per_slab = rem / cache->object_size;

    if (objects_per_slab < 8)
        return -1;
    return objects_per_slab;
}

static int calc_object_align(const size_t size) {
    if (size == 0)
        return -1;
    const size_t largest_pow2 = size & (-size); // isolates lowest set bit
    return (int)(largest_pow2 < 16 ? largest_pow2 : 16);
}

static uint32_t calc_slab_order(const size_t size) {
    // makes enough space for at least 8 objects per slab
    if (size == 0)
        return -1;

    const size_t min_size =
        size * 8 + sizeof(slab_descriptor) + 16; // 16 for max alignment padding
    const uint32_t pages_needed_size = align_up(min_size, PAGE_SIZE);
    const uint32_t slab_order = buddy_order(pages_needed_size);
    return slab_order;
}

static kmem_cache* kmem_cache_create(kmem_cache* cache, const size_t size) {
    /*
     * initialise a kmem_cache by:
     * calculating object_align, objects_per_slab,
     * and then assigning a slab_order based on objects_per_slab
     * this is never exposed to kernellib, only kmem_cache_create_sl is exposed
     * this is called in slab_init and therefore does not need spinlocks
     */
    cache->object_size = size;

    cache->object_align = calc_object_align(size);
    if (cache->object_align == (uint32_t)-1)
        return NULL;

    const uint32_t slab_order = calc_slab_order(size);
    if (slab_order == (uint32_t)-1)
        return NULL;
    cache->slab_order = slab_order;

    const uint64_t objects_per_slab = calc_objects_per_slab(cache);
    if (objects_per_slab == (uint64_t)-1)
        return NULL;

    cache->objects_per_slab = objects_per_slab;

    // create three empty free/partial/full slab lists
    cache->free_slabs.next = &cache->free_slabs;
    cache->free_slabs.prev = &cache->free_slabs;

    cache->partial_slabs.next = &cache->partial_slabs;
    cache->partial_slabs.prev = &cache->partial_slabs;

    cache->full_slabs.next = &cache->full_slabs;
    cache->full_slabs.prev = &cache->full_slabs;

    return cache;
}

static uint32_t size_to_index(const size_t size) {
    // should return log2(roundup(size))
    if (size == 0)
        return -1;
    const size_t clamped =
        size < MIN_SLAB_ALLOC_SIZE ? MIN_SLAB_ALLOC_SIZE : size;
    const size_t nearest_pow2 = round_up_pow2(clamped);
    return klog2(nearest_pow2) - klog2(MIN_SLAB_ALLOC_SIZE);
}

status_t __init_slab() {
    spinlock_init(&slab_lock);
    for (int pow = 4; pow <= 11; pow++) {
        kmem_cache* curr = kmem_cache_create(
            &slab_caches[size_to_index(1ULL << pow)], (1ULL << pow));
        if (curr == NULL)
            return STATUS_ERROR;
    }

    vesa_nt_println("Initialized slab allocator caches...\0", 2, 0);
    return STATUS_OK;
}

// TODO: does this need to lock slab_lock??
static status_t kmem_cache_grow(kmem_cache* cache) {
    // grows a kmem_cache with another buddy_alloc((1ULL <<
    // kmem_cache->slab_order))
    slab_descriptor* descriptor =
        (slab_descriptor*)buddy_kalloc((1ULL << cache->slab_order) * PAGE_SIZE);
    if (descriptor == NULL)
        return STATUS_ERROR;
    descriptor->cache = cache;
    descriptor->inuse_num = 0;

    // thread a freelist and insert descriptor to previous slab's list
    const uint64_t first_object_offset =
        (sizeof(slab_descriptor) + cache->object_align - 1) &
        ~(cache->object_align - 1);
    void* new_slab_first_object =
        (void*)((uint64_t)descriptor + first_object_offset);

    for (uint32_t idx = 0; idx < cache->objects_per_slab; idx++) {
        void* object_addr =
            (void*)((uint64_t)new_slab_first_object + idx * cache->object_size);
        if (idx < cache->objects_per_slab - 1) {
            *(void**)object_addr =
                (void*)((uint64_t)object_addr +
                        cache->object_size); // setNext(object +
                                             // sizeof(object));
        } else {
            *(void**)object_addr = NULL; // setNext(NULL);
        }
    }
    descriptor->free_list = new_slab_first_object;
    // void* next = *(void**)descriptor->free_list;
    // this is how you read 'descriptor->free_list->next'
    // since free_list is of type void*

    // add new slab_descriptor to free slabs list in kmem_cache
    descriptor->slab_lists_node.next = cache->free_slabs.next;
    descriptor->slab_lists_node.prev = &cache->free_slabs;
    cache->free_slabs.next->prev = &descriptor->slab_lists_node;
    cache->free_slabs.next = &descriptor->slab_lists_node;

    return STATUS_OK;
}

// these functions exist for those concerned about optimisation
// they are just slab_alloc/free, but without the size to index translation
// they take a cache from which the caller wants to allocate
// and give an object from the free or partial slabs
void* kmem_cache_kalloc(kmem_cache* cache) {
    spinlock_acquire(&slab_lock);
    slab_descriptor* desc;

    if (cache->partial_slabs.next != &cache->partial_slabs) {
        desc = container_of(cache->partial_slabs.next, slab_descriptor,
                            slab_lists_node);
    } else if (cache->free_slabs.next != &cache->free_slabs) {
        desc = container_of(cache->free_slabs.next, slab_descriptor,
                            slab_lists_node);
    } else {
        spinlock_release(&slab_lock);
        const status_t status = kmem_cache_grow(cache);
        spinlock_acquire(&slab_lock);
        if (status != STATUS_OK) {
            spinlock_release(&slab_lock);
            return NULL;
        }
        desc = container_of(cache->free_slabs.next, slab_descriptor,
                            slab_lists_node);
    }
    // by now we have a slab descriptor and only need to pop from its free list
    void* ptr = NULL;

    // caller must check if returned ptr is null
    ptr = desc->free_list;
    if (ptr != NULL)
        desc->free_list = *(void**)ptr; // get_next
    else {
        spinlock_release(&slab_lock);
        return NULL;
    }
    desc->inuse_num++;
    if (desc->inuse_num == cache->objects_per_slab) {
        // move from free (cache->objects_per_slab = 1) to full
        // or more probably: move from partial to full
        desc->slab_lists_node.prev->next = desc->slab_lists_node.next;
        desc->slab_lists_node.next->prev = desc->slab_lists_node.prev;

        // insert desc at front of full slabs
        desc->slab_lists_node.next = cache->full_slabs.next;
        desc->slab_lists_node.prev = &cache->full_slabs;
        cache->full_slabs.next->prev = &desc->slab_lists_node;
        cache->full_slabs.next = &desc->slab_lists_node;
    } else if (desc->inuse_num == 1) {
        // move from free to partial
        // remove desc from free
        desc->slab_lists_node.prev->next = desc->slab_lists_node.next;
        desc->slab_lists_node.next->prev = desc->slab_lists_node.prev;

        // insert at front of partial slabs
        desc->slab_lists_node.next = cache->partial_slabs.next;
        desc->slab_lists_node.prev = &cache->partial_slabs;
        cache->partial_slabs.next->prev = &desc->slab_lists_node;
        cache->partial_slabs.next = &desc->slab_lists_node;
    }

    spinlock_release(&slab_lock);
    return ptr;
}

void kmem_cache_kfree(kmem_cache* cache, void* object) {
    spinlock_acquire(&slab_lock);
    const uint32_t slab_order = cache->slab_order;
    const uint32_t slab_size = PAGE_SIZE << slab_order;
    const uint64_t slab_base = (uint64_t)object & ~((uint64_t)slab_size - 1);
    slab_descriptor* desc = (slab_descriptor*)slab_base;

    desc->inuse_num--;
    if (desc->inuse_num == 0) {
        // move slab from partial to free
        // remove from partial slabs
        // if cache->objects_per_slab = 1,
        // then this removes from full slabs and inserts to free slabs instead
        desc->slab_lists_node.prev->next = desc->slab_lists_node.next;
        desc->slab_lists_node.next->prev = desc->slab_lists_node.prev;

        // insert to free slabs
        desc->slab_lists_node.next = cache->free_slabs.next;
        desc->slab_lists_node.prev = &cache->free_slabs;
        cache->free_slabs.next->prev = &desc->slab_lists_node;
        cache->free_slabs.next = &desc->slab_lists_node;
    } else if (desc->inuse_num == cache->objects_per_slab - 1) {
        // move from full slabs to partial slabs
        // remove from full slabs
        desc->slab_lists_node.prev->next = desc->slab_lists_node.next;
        desc->slab_lists_node.next->prev = desc->slab_lists_node.prev;

        // insert to partial slabs
        desc->slab_lists_node.next = cache->partial_slabs.next;
        desc->slab_lists_node.prev = &cache->partial_slabs;
        cache->partial_slabs.next->prev = &desc->slab_lists_node;
        cache->partial_slabs.next = &desc->slab_lists_node;
    }

    *(void**)object = desc->free_list;
    desc->free_list = object;
    spinlock_release(&slab_lock);
}

void* slab_kalloc(const size_t size) {
    // select cache and then pop from the free/partial slab's free lists head
    // node doesn't yet handle caches outside the statically allocated caches
    if (size > MAX_SLAB_ALLOC_SIZE)
        return NULL;

    const uint32_t index = size_to_index(size);
    if (index > NUM_CACHES) {
        return NULL;
    }

    kmem_cache* cache = &slab_caches[index];
    return kmem_cache_kalloc(cache);
}

void slab_kfree(void* object, const size_t size) {
    const uint32_t index = size_to_index(size);
    kmem_cache* cache = &slab_caches[index];
    kmem_cache_kfree(cache, object);
}

kmem_cache* kmem_cache_create_sl(kmem_cache* cache, const size_t size) {
    /* initialise a kmem_cache by:
     * calculating object_align, objects_per_slab,
     * and then assigning a slab_order based on objects_per_slab
     * this is exposed to kernellib, and should be used for creating caches
     * optimised for kernel structs/objects this runs after APs are turned on
     * and therefore requires spinlocks
     */
    spinlock_acquire(&slab_lock);
    cache->object_size = size;
    cache->object_align = calc_object_align(size);
    if (cache->object_align == (uint32_t)-1)
        return NULL;

    const uint32_t slab_order = calc_slab_order(size);
    cache->slab_order = slab_order;

    const uint64_t objects_per_slab = calc_objects_per_slab(cache);
    if (objects_per_slab == (uint64_t)-1)
        return NULL;

    cache->objects_per_slab = objects_per_slab;

    // create three empty free/partial/full slab lists
    cache->free_slabs.next = &cache->free_slabs;
    cache->free_slabs.prev = &cache->free_slabs;

    cache->partial_slabs.next = &cache->partial_slabs;
    cache->partial_slabs.prev = &cache->partial_slabs;

    cache->full_slabs.next = &cache->full_slabs;
    cache->full_slabs.prev = &cache->full_slabs;

    spinlock_release(&slab_lock);
    return cache;
}

/*
 * phys_kmalloc wrapper functions
 * init wrapper, kmalloc and kfree dispatcher functions
 * one may call slab_kfree/alloc or buddy_kfree/alloc functions manually
 * also possible to call kmem_cache_alloc/free functions
 * TODO:
 * 1. separate pmm_lock to buddy_lock and slab_lock
 * 2. let slab_kalloc and slab_kfree look in new caches for allocating and
 * freeing
 */

void __init_phys_kmalloc() {
    // returns the status code for __init_slab()
    __init_buddy();
    status_t status = __init_slab();
    if (status != STATUS_OK)
        PANIC("ALLOCATOR INITIALISATION FAILURE, CANNOT BOOT KERNEL\0");
    vesa_nt_println("Allocator initialisation success\0", 3, 0);
}

void* phys_kmalloc(const size_t size, const ALLOC_FLAG flg) {
    switch (flg) {
    case PAGEFRAME_ALLOC:
        return buddy_kalloc(size);
    case SLAB_ALLOC:
        return slab_kalloc(size);
    default:
        if (size <= MAX_SLAB_ALLOC_SIZE)
            return slab_kalloc(size);
        return buddy_kalloc(size);
    }
}
// allocation functions that only take a size and then allcoate from the slab
// allocator are flawed as they cannot allocate from a kmem_cache that wasnt
// initialised by default solution is to either take a kmem_cache* argument in
// phys_kmalloc that could be NULL maybe ALLOC_FLAG should have some option
// like: SLAB_WITH_CACHE

void phys_kfree(void* ptr, const size_t size, const ALLOC_FLAG flg) {
    switch (flg) {
    case PAGEFRAME_FREE:
        buddy_kfree(ptr, size);
        break;
    case SLAB_FREE:
        slab_kfree(ptr, size);
        break;
    default:
        if (size <= MAX_SLAB_ALLOC_SIZE)
            slab_kfree(ptr, size);
        else
            buddy_kfree(ptr, size);
    }
}
