/* Eyal Kaghanovich
 * 
 * 
*/


#include "include/vma_rb_tree.h"
#include "include/mikroKernellib-common.h"
#include "include/spinlocks.h"
#include "include/mmu_page_tables.h"
#include "include/vesa_graphics_lib.h"
#include "include/phys_kmalloc.h"
#include <stdint.h>


typedef struct rb_node {
    uint64_t* vma_start;
    uint64_t* vma_end;
    uint64_t vma_pte;

    uint32_t color;
    struct rb_node* right;
    struct rb_node* left;
    struct rb_node* parent;
} rb_node_t;


void __init_vma_rb_tree(void /*pml4/5t status variable?*/) {
    
}