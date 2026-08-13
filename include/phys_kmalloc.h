
#pragma once

#include "include/mikroKernellib-common.h"
#include <stddef.h>
#include <stdint.h>

// TODO: change flag usage to enum usage

#define PAGEFRAME_ALLOC 0b00000001
#define BUDDY_ALLOC 0b00000001
#define SLAB_ALLOC 0b00000010
#define CHOOSE_ALLOC 0b00000000

#define BUDDY_FREE 0b00000001
#define PAGEFRAME_FREE 0b00000000
#define SLAB_FREE 0b00000010
#define CHOOSE_FREE 0b00000000

// #define SPECIAL_ALLOC 0b11111111

#define ALLOC_FLAG flag
typedef uint8_t flag;
// if flg = 0b00000000 -> let the allocator decide on strategy

#define ONE_MB_ADDRESS 0x100000

#define FREE 0
#define USED 1

#define MIN_SLAB_ALLOC_SIZE \
    16                           // size of a double linked list node {void* n, void* p}
#define MAX_SLAB_ALLOC_SIZE 2048 // half of PAGE_SIZE

/*
    two main physical allocator function bodies, that depending on size
   requested and flg: decide on strategy then call allocator for selected
   strategy, or fall back to default strategy. if flg = 0x00 -> let the
   allocator choose a strategy if flg = 0x01 -> buddy, if flg = 0x02 -> slab if
   flg != 0x00 or 0x01 or 0x02 -> let the allocator choose if flg == 0xff -> use
   special strategy, prone to problems/inefficiencies ~probably
*/

typedef struct {
    void* base_address;
    uint64_t chunk_size;
    uint32_t type;
    uint32_t ACPI_ext_attr;
} __attribute__((packed)) mmap_entry_0xe820;

void __init_phys_kmalloc(void);
void* phys_kmalloc(size_t size, ALLOC_FLAG flg);
void phys_kfree(void* ptr, size_t size, ALLOC_FLAG flg);

void __init_buddy(void);
void* buddy_kalloc(size_t size);
void buddy_kfree(void* chunk, size_t size);

status_t __init_slab(void);
void* slab_kalloc(size_t size);
void slab_kfree(void* object, size_t size);

typedef struct slab_list_node slab_list_node;
typedef struct kmem_cache kmem_cache;

typedef struct slab_list_node {
    slab_list_node* next;
    slab_list_node* prev;
} slab_list_node;

typedef struct {
    uint32_t inuse_num;
    void* free_list;
    kmem_cache* cache;
    slab_list_node slab_lists_node;
} slab_descriptor;

typedef struct kmem_cache {
    size_t object_size;
    uint32_t object_align;
    uint8_t slab_order;
    uint32_t objects_per_slab;

    slab_list_node free_slabs;
    slab_list_node partial_slabs;
    slab_list_node full_slabs;
} kmem_cache;

kmem_cache* kmem_cache_create_sl(kmem_cache* cache, size_t size);
void* kmem_cache_kalloc(kmem_cache* cache);
void kmem_cache_kfree(kmem_cache* cache, void* object);
