#pragma once



#include "include/rb_tree.h"
#include <stddef.h>
#include <stdint.h>

void __init_virt_kmalloc();

void* virt_kmalloc(size_t size);

void virt_kfree(void* ptr);


enum STACK_SIZE_FLAG {
    STACK_SIZE_SMALL,
    STACK_SIZE_MEDIUM,
    STACK_SIZE_LARGE
};

// structs for rb-tree
// to keep track of allocations

struct vac {
    uint64_t addr;
    uint64_t end;
    struct rb_node node;
};

typedef struct vac vac_t;

// vac = virtual allocation chunk
int vac_cmp(struct rb_node* a, struct rb_node* b);
