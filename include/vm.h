#pragma once

#include "include/rb_tree.h"
#include "include/spinlocks.h"
#include <stdint.h>

typedef uint64_t offset_t;

typedef struct vma {
    uint64_t vma_start;
    uint64_t vma_end;
    uint64_t vma_pte;
    struct rb_node node;
} vma_t;

struct proc_memory {
    struct rb_node* vma_tree;
    spinlock_t lock;
};

void vm_handle_page_fault(uint64_t addr);

vma_t* rb_find_vma(struct rb_node** root, uint64_t addr);

int vma_cmp(struct rb_node* a, struct rb_node* b);
