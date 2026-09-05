/* Eyal Kaghanovich
 * core virtual memory functionality:
 * mmap internal implementation, page fault handler, ...
 */

#include "include/vm.h"
#include "include/mikroKernellib-common.h"
#include "include/mmu.h"
#include "include/rb_tree.h"
#include "include/tasks.h"
#include "include/per_cpu.h"

// finding adjacent VMAs:
static inline vma_t* rb_successor(struct rb_node** root, const uint64_t addr) {
    // returns the vma of the leftmost node with addr < vma_start
    struct rb_node* current = *root;
    struct rb_node* successor = NULL;
    while (current != NULL) {
        vma_t* v = container_of(current, vma_t, node);
        if (addr < directmap_p2v(v)->vma_start) {
            // go left
            successor = current;
            current = directmap_p2v(current)->left;
        } else {
            // go right
            current = directmap_p2v(current)->right;
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
        if (addr > directmap_p2v(v)->vma_start) {
            // go right
            predecessor = current;
            current = directmap_p2v(current)->right;
        } else {
            // go left
            current = directmap_p2v(current)->left;
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
        if (addr < directmap_p2v(v)->vma_start) {
            current = directmap_p2v(current)->left;
        } else if (addr >= directmap_p2v(v)->vma_end) {
            current = directmap_p2v(current)->right;
        } else {
            return v;
        }
    }
    return NULL;
}

int vma_cmp(struct rb_node* a, struct rb_node* b) {
    const struct vma* va = container_of(a, struct vma, node);
    const struct vma* vb = container_of(b, struct vma, node);
    if (directmap_p2v(va)->vma_start < directmap_p2v(vb)->vma_start)
        return -1;
    if (directmap_p2v(va)->vma_start > directmap_p2v(vb)->vma_start)
        return 1;
    return 0;
}

void* vm_mmap(void* addr, size_t length, int prot, int flags, int fd,
              offset_t offset) {
    // add VMA region for the calling process' VMA tree
    // the context for this to be called is after a userland process calls
    // mmap() and switches from userland privilege to kernel mode we need to
    // follow current task_struct to know which VMA rb-tree root to modify

    // we do not need to actually map anything
    // in demand paging mmap just adds the vma to the tree but whenever memory
    // in that region is accessed the page fault handler will map the physical
    // page to the process as demanded, depends on mapping

    struct task_struct* current = cpu_get_current_task();
    struct mm* task_mm = current->task_mm;
    struct rb_node* rb_root = task_mm->vma_tree;
    offset_t phys_pml4t = task_mm->phys_pml4t;

    // validation...


}

static void vm_handle_userland_fault(const uint64_t addr, struct mm* task_mm) {}

void vm_handle_page_fault(const uint64_t addr) {
    if (addr < PML4_KERNEL_HALF_BASE) {
        vm_handle_userland_fault(
            addr,
            &(struct mm){.vma_tree = 0, .mm_lock = 0});
        return;
    }
    struct task_struct* current = cpu_get_current_task();
    struct mm* task_mm = current->task_mm;
    struct rb_node* rb_root = task_mm->vma_tree;

}
