

#include "include/phys_kmalloc.h"
#include "include/vga-graphics.h"
#include "include/mikroKernellib-common.h"

#define NUM_MAXORDER (metadata->max_order + 1) // 11

extern uint64_t _kernel_end;
uint64_t kernel_end = (uint64_t)&_kernel_end;


static void memset_lb(void* ptr, uint8_t val, const uint64_t size) {
    uint8_t* p = (uint8_t*)ptr;
    for (uint64_t i = 0; i < size; i++) {
        p[i] = val;
    }
}


typedef struct {
    void* base_address;
    uint64_t chunk_size;
    uint32_t type;
    uint32_t ACPI_ext_attr;
} __attribute__((packed)) mmap_entry_0xe820;
// packed is not very important here

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
    uint32_t order_offsets[11]; // order_offsets[k] = number of bitmap bits belonging to orders below k
} __attribute__((packed)) buddy_metadata;

extern uint16_t mmap_entry_count;
extern mmap_entry_0xe820 mmap_bios_entries[];


void buddy_init() {
    vga_print_mmap(17, 0);
    buddy_metadata* metadata = (buddy_metadata*)kernel_end;
    metadata->min_order = 0;
    metadata->max_order = 10;
    metadata->free_lists_ptr = (buddy_chunk**)((buddy_metadata*)kernel_end + 1);
    memset_lb(metadata->free_lists_ptr, 0x00, NUM_MAXORDER * sizeof(buddy_chunk*));

    void* best_base_addr = 0;
    uint64_t best_chunk_size = 0;
    // now we need to walk over e820
    for (int idx = 0; idx < mmap_entry_count; idx++) {
        if (mmap_bios_entries[idx].type != 0x01) continue;

        void* coalescence_base = mmap_bios_entries[idx].base_address;
        uint64_t coalescence_size = mmap_bios_entries[idx].chunk_size;

        // coalesce contiguous type 0x01 neighbors
        while (idx + 1 < mmap_entry_count &&
               mmap_bios_entries[idx + 1].type == 0x01 &&
               (uint64_t)mmap_bios_entries[idx + 1].base_address == (uint64_t)coalescence_base + coalescence_size
              ) {
            coalescence_size += mmap_bios_entries[++idx].chunk_size;
        }

        if (coalescence_size > best_chunk_size) {
            best_chunk_size = coalescence_size;
            best_base_addr = coalescence_base;
        }
    }

    const uint64_t worst_case_bitmap = (best_chunk_size / PAGE_SIZE) / 8;
    // best_base_addr must clear: 1MB low memory, kernel image, and metadata sitting after kernel
    uint64_t lower_bound = ONE_MB_ADDRESS;
    const uint64_t after_metadata = kernel_end + sizeof(buddy_metadata) +
        NUM_MAXORDER * sizeof(buddy_chunk*) + worst_case_bitmap;

    if (after_metadata > lower_bound) lower_bound = after_metadata;
    //if kernel binary + metadata > 1MB => allocate memory from after kernel + metadata

    if ((uint64_t)best_base_addr < lower_bound) {
        if (lower_bound < (uint64_t)best_base_addr + best_chunk_size) {
            best_chunk_size -= lower_bound - (uint64_t)best_base_addr;
            best_base_addr = (void*)lower_bound;
        }
    }

    // align best_base_addr to 4MB, largest order allocation
    //important for xor buddy addressing magic
    const uint64_t base = (uint64_t)best_base_addr;
    const uint64_t max_order_align = (1ULL << metadata->max_order) * PAGE_SIZE;
    const uint64_t align = (max_order_align - (base & (max_order_align - 1))) & (max_order_align - 1);
    best_base_addr = (void*)(base + align);
    best_chunk_size -= align;
    best_chunk_size = (best_chunk_size) & ~(PAGE_SIZE - 1);
    // this reduces the size of the memory the buddy manages
    // but, it makes sure it's divisible by PAGE_SIZE

    metadata->base_address = best_base_addr;
    metadata->size = best_chunk_size;

    // we now need free lists array and bitmap...
    // we need to split our contiguous chunk into smaller power of two chunks and insert them to free lists
    uint32_t counts[NUM_MAXORDER];
    for (uint8_t i = 0; i < NUM_MAXORDER; i++) counts[i] = 0;

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
        if (order < metadata->min_order) break;

        const uint64_t chunk_bytes = (1ULL << order) * PAGE_SIZE;
        counts[order]++;
        addr += chunk_bytes;
        remaining -= chunk_bytes;
    }


    buddy_chunk* current_addr = best_base_addr;
    for (int order = metadata->max_order; order >= 0; order--) {
        buddy_chunk* head = current_addr;
        const uint32_t count = counts[order];

        if (count == 0) continue;

        for (uint32_t i = 0; i < count; i++) {
            buddy_chunk* next_chunk = (struct buddy_chunk*)((uint8_t*)current_addr + (1ULL << order) * PAGE_SIZE);
            current_addr->next = i == count - 1 ? NULL : next_chunk;
            current_addr->prev = i == 0 ? NULL : (buddy_chunk*)((uint8_t*)current_addr - (1ULL << order) * PAGE_SIZE);
            current_addr = next_chunk;
        }

        metadata->free_lists_ptr[order] = head;
    }

    const uint64_t num_pages = metadata->size / PAGE_SIZE;
    const uint64_t bitmap_size_bytes = num_pages / 8;

    metadata->bitmap_ptr = (void*)(kernel_end + sizeof(buddy_metadata) +
        NUM_MAXORDER * sizeof(buddy_chunk*));
    // puts bitmap after everything it needs to be
    // now we need to memset the bitmap
    memset_lb(metadata->bitmap_ptr, 0x00, bitmap_size_bytes);

    // precompute order_offsets: order_offsets[k] = total bits used by orders 0..k-1
    metadata->order_offsets[0] = 0;
    for (int k = 1; k < NUM_MAXORDER; k++) {
        const uint32_t bits_at_prev = num_pages >> k; // buddy pairs at order k-1
        metadata->order_offsets[k] = metadata->order_offsets[k - 1] + bits_at_prev;
    }

    vga_nt_println("Initialized buddy allocation...\0", 1, 0);
}
// I really gotta do one bit per buddy someday...

void update_bitmap_lb(const buddy_metadata* metadata, const uint64_t addr, const uint8_t order, const uint8_t status) {
    uint8_t* bitmap = (uint8_t*)metadata->bitmap_ptr;
    const uint32_t block_idx = (addr - (uint64_t)metadata->base_address) >> (12 + order);
    const uint32_t bit_pos = metadata->order_offsets[order] + block_idx;
    const uint32_t byte_idx = bit_pos / 8;
    const uint8_t bit_offset = bit_pos % 8;
    bitmap[byte_idx] = (bitmap[byte_idx] & ~(1 << bit_offset)) | (status << bit_offset);
}


void* buddy_alloc(const size_t size) {
    const buddy_metadata* metadata = (buddy_metadata*)kernel_end;
    uint8_t order = 0;
    while (size > (1ULL << order) * PAGE_SIZE) order++;
    if (order > metadata->max_order) return NULL; // if requested size is too big return null

    for (int idx = order; idx < NUM_MAXORDER; idx++) {
        if (metadata->free_lists_ptr[idx] == NULL) continue;

        // pop block from free list
        buddy_chunk* chunk = metadata->free_lists_ptr[idx];
        metadata->free_lists_ptr[idx] = chunk->next;
        if (chunk->next != NULL) chunk->next->prev = NULL;
        chunk->next = NULL;

        // split
        for (int level = idx; level > order; level--) {
            buddy_chunk* buddy = (buddy_chunk*)((uint8_t*)chunk + (1ULL << (level - 1)) * PAGE_SIZE); // buddy after split
            buddy->prev = NULL; // will be at head so prev must be null
            buddy->next = metadata->free_lists_ptr[level - 1]; // since we are inserting at head of the lower list, next must be the current head
            if (buddy->next != NULL) buddy->next->prev = buddy; // if it's the only chunk in the list of current order
            metadata->free_lists_ptr[level - 1] = buddy; // free lists ptr points to buddy
        }

        update_bitmap_lb(metadata, (uint64_t)chunk, order, USED);
        return (void*)chunk;
    }

    return NULL;
}


void buddy_free(void* chunk, const size_t size) {
    const buddy_metadata* metadata = (buddy_metadata*)kernel_end;

    uint8_t order = 0;
    while (size > (1ULL << order) * PAGE_SIZE) order++;
    if (order > metadata->max_order) return;

    update_bitmap_lb(metadata, (uint64_t)chunk, order, FREE);
    const uint64_t* bitmap = (uint64_t*)metadata->bitmap_ptr;

    while (order < metadata->max_order) {
        const uint32_t block_idx = ((uint8_t*)chunk - (uint8_t*)metadata->base_address) >> (12 + order);
        const uint32_t bit_pos = metadata->order_offsets[order] + block_idx;
        const uint32_t buddy_bit_idx = bit_pos ^ 1;

        const uint64_t buddy_bit = bitmap[buddy_bit_idx / 64] & (1ULL << (buddy_bit_idx % 64));
        if (buddy_bit != 0) break; // buddy is allocated, can't coalesce

        // buddy address via xor, works because base is aligned to max order
        buddy_chunk* buddy = (buddy_chunk*)((uint64_t)chunk ^ ((1ULL << order) * PAGE_SIZE));

        // unlink buddy from free list
        if (buddy->prev != NULL) buddy->prev->next = buddy->next;
        else metadata->free_lists_ptr[order] = buddy->next;
        if (buddy->next != NULL) buddy->next->prev = buddy->prev;
        buddy->next = NULL;
        buddy->prev = NULL;

        // coalesced block is at the lower address
        if ((uint64_t)buddy < (uint64_t)chunk) chunk = (void*)buddy;

        order++;
    }

    // insert coalesced block into free list at final order
    buddy_chunk* block = chunk;
    block->prev = NULL;
    block->next = metadata->free_lists_ptr[order];
    if (block->next != NULL) block->next->prev = block;
    metadata->free_lists_ptr[order] = block;
}