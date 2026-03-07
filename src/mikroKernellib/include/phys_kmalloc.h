
#pragma once

#include <stdint.h>
#include <stddef.h>



#define BUDDY_ALLOC 0b00000001
#define SLAB_ALLOC 0b00000010
#define CHOOSE_ALLOC 0b00000000
#define SPECIAL_ALLOC 0b11111111

#define ALLOC_FLAG flg
typedef uint8_t flg;
// if flg = 0b00000000 -> let the allocator decide on strategy

#define ONE_MB_ADDRESS 0x100000

#define FREE 0
#define USED 1

/*
    two main physical allocator function bodies, that depending on size requested and flg:
    decide on strategy then call allocator for selected strategy, or fall back to default strategy.
    if flg = 0x00 -> let the allocator choose a strategy
    if flg = 0x01 -> buddy, if flg = 0x02 -> slab
    if flg != 0x00 or 0x01 or 0x02 -> let the allocator choose
    if flg == 0xff -> use special strategy, prone to problems/inefficiencies ~probably
*/
void* phys_kmalloc(size_t sz, ALLOC_FLAG flg);
void phys_kfree(void* ptr, ALLOC_FLAG flg);


// buddy and slab allocator implementations
void buddy_init();
void* buddy_alloc(size_t size);
void buddy_free(void* chunk, size_t size);


void* slab_alloc(size_t sz);
void slab_free(void* ptr);