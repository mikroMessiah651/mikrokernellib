/* Eyal Kaghanovich
 * core virtual memory functionality:
 * mmap internal implementation, page fault handler, ...
 */

#include "include/vm.h"
#include "include/mikroKernellib-common.h"
#include "include/mmu.h"
#include "include/rb_tree.h"
#include "include/spinlocks.h"
#include <stdint.h>

static inline uint64_t* get_core_pml4t(void) {
    uint64_t cr3;
    get_cr3(cr3);

    uint64_t* pml4 = (uint64_t*)(cr3 & ~0xFFFULL);
    return pml4;
}

// finding adjacent VMAs:
static inline vma_t* rb_successor(struct rb_node** root, const uint64_t addr) {
    // returns the vma of the leftmost node with addr < vma_start
    struct rb_node* current = *root;
    struct rb_node* successor = NULL;
    while (current != NULL) {
        vma_t* v = container_of(current, vma_t, node);
        if (addr < v->vma_start) {
            // go left
            successor = current;
            current = current->left;
        } else {
            // go right
            current = current->right;
        }
    }
    return successor ? container_of(successor, vma_t, node) : NULL;
}

static inline vma_t* rb_predeccessor(struct rb_node** root,
                                     const uint64_t addr) {
    // returns the vma of the rightmost node with vma_start < addr
    struct rb_node* current = *root;
    struct rb_node* predecessor = NULL;
    while (current != NULL) {
        vma_t* v = container_of(current, vma_t, node);
        if (addr > v->vma_start) {
            // go right
            predecessor = current;
            current = current->right;
        } else {
            // go left
            current = current->left;
        }
    }
    return predecessor ? container_of(predecessor, vma_t, node) : NULL;
}

vma_t* rb_find_vma(struct rb_node** root, const uint64_t addr) {
    // returns the vma struct of the node which represents the VMA where addr
    // falls in
    struct rb_node* current = *root;
    while (current != NULL) {
        vma_t* v = container_of(current, vma_t, node);
        if (addr < v->vma_start) {
            current = current->left;
        } else if (addr >= v->vma_end) {
            current = current->right;
        } else {
            return v;
        }
    }
    return NULL;
}

int vma_cmp(struct rb_node* a, struct rb_node* b) {
    const struct vma* va = container_of(a, struct vma, node);
    const struct vma* vb = container_of(b, struct vma, node);
    if (va->vma_start < vb->vma_start)
        return -1;
    if (va->vma_start > vb->vma_start)
        return 1;
    return 0;
}

void* vm_mmap(void* addr, size_t length, int prot, int flags, int fd,
              offset_t offset) {
    // add region for the calling process' VMA tree
    // the context for this to be called is after a userland process calls
    // mmap() and switches from userland privilege to kernel mode we need to
    // follow current task_struct to know which VMA rb-tree root to modify

    // struct task_struct* current = get_current_task();
    // struct task_mm* proc_mm = current->proc_mm;
    // struct rb_node* rb_root = proc_mm->vma_tree;

    // we do not need to actually map anything
    // in demand paging mmap just adds the vma to the tree but whenever memory
    // in that region is accessed the page fault handler will map the physical
    // page to the process as demanded, depends on mapping
}

void vm_handle_page_fault(uint64_t addr) {
    // struct task_struct* current = get_current_task();
    // struct task_mm* proc_mm = current->proc_mm;
    // struct rb_node* rb_root = proc_mm->vma_tree;
}
