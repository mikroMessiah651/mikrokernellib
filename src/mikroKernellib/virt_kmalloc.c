/* Eyal Kaghanovich
 * virtual kernel memory allocator
 */


#include "include/virt_kmalloc.h"
 #include "include/mikroKernellib-common.h"
#include "include/phys_kmalloc.h"
#include "include/spinlocks.h"
#include <stddef.h>
#include <stdint.h>


static struct rb_node* vac_rb_root = NULL;
static kmem_cache vac_cache;
static spinlock_t vac_rb_spinlock;


// rb_tree<vma_t> t;
static inline void rb_insert_vac(vac_t* v) {
    // builds a node for the vma and inserts it to the vma rb-tree
    // passes vma comparator and kernel vma rb tree root
    rb_insert(&vac_rb_root, &v->node, vac_cmp);
}

static inline void rb_remove_vac(vac_t* v) {
    rb_delete(&vac_rb_root, &v->node, vac_cmp);
}

int vac_cmp(struct rb_node* a, struct rb_node* b) {
    vac_t* a_vac = container_of(a, struct vac, node);
    vac_t* b_vac = container_of(b, struct vac, node);
    if (a_vac->addr > b_vac->addr)
        return 1;
    else if (a_vac->addr < b_vac->addr)
        return -1;
    return 0;
}

// finding adjacent VMAs:
static vac_t* rb_successor_vac(struct rb_node** root, const uint64_t addr) {
    // returns the vma of the leftmost node with addr < vma_start
    struct rb_node* current = *root;
    struct rb_node* successor = NULL;
    while (current != NULL) {
        vac_t* v = container_of(current, vac_t, node);
        if (addr < v->addr) {
            // go left
            successor = current;
            current = current->left;
        } else {
            // go right
            current = current->right;
        }
    }
    return successor ? container_of(successor, vac_t, node) : NULL;
}

static vac_t* rb_predeccessor_vac(struct rb_node** root, const uint64_t addr) {
    // returns the vma of the rightmost node with vma_start < addr
    struct rb_node* current = *root;
    struct rb_node* predecessor = NULL;
    while (current != NULL) {
        vac_t* v = container_of(current, vac_t, node);
        if (addr > v->addr) {
            // go right
            predecessor = current;
            current = current->right;
        } else {
            // go left
            current = current->left;
        }
    }
    return predecessor ? container_of(predecessor, vac_t, node) : NULL;
}

static vac_t* rb_find_vac(struct rb_node** root, const uint64_t addr) {
    // returns the vma struct of the node which represents the VMA where addr
    // falls in
    struct rb_node* current = *root;
    while (current != NULL) {
        vac_t* v = container_of(current, vac_t, node);
        if (addr < v->addr) {
            current = current->left;
        } else if (addr >= v->end) {
            current = current->right;
        } else {
            return v;
        }
    }
    return NULL;
}

void __init_virt_kmalloc(void) {
    spinlock_init(&vac_rb_spinlock);
    kmem_cache_create_sl(&vac_cache, sizeof(struct vac));
}

void* virt_kmalloc(const size_t size) {
    // virt_kmalloc supports 4KB or above allocations,
    // and uses the buddy allocator as the underlying mm
    // allocations via virt_kmalloc must be eagerly mapped
    // such that a page fault in a kernel thread in vmalloc region
    // panics the thread
}

void virt_kfree(void* ptr) {
}


// not yet implemented
// create means to allocate and map something

// tmp
typedef uint8_t kthread_desc;
typedef uint8_t kmodule_desc;

void* vm_create_kthread_stack(const kthread_desc thread);
// reads STACK_SIZE_FLAG from the descriptor

void vm_destroy_kthread_stack(const kthread_desc thread);
// makes sure to deallocate and unmap correct sizes, reads STACK_SIZE_FLAG from the desc

void* vm_create_module_space(const kmodule_desc module);
// reads kernel module size, does not do the elf mapping, it just allcoates and maps the memory
// the actual permissions, region offsets... all that is handled by the elf loader

void vm_destroy_module_space(const kmodule_desc module);
