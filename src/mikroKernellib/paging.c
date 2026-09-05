/* Eyal Kaghanovich
 * kernel paging setup, initial page tables and VMA rb-tree
 * kernel mapping functions...
 */

#include "include/paging.h"
#include "include/mapped_phys_kmalloc.h"
#include "include/mikroKernellib-common.h"
#include "include/mmu.h"
#include "include/phys_kmalloc.h"
#include "include/rb_tree.h"
#include "include/vm.h"
#include <stddef.h>
#include <stdint.h>

extern uint64_t _kernel_start;
extern uint64_t _kernel_end;
extern uint64_t _text_end;
extern uint64_t _rodata_start;
extern uint64_t _rodata_end;
extern uint64_t _data_start;
extern uint64_t _bss_start;

static uint64_t kernel_start_addr = (uint64_t)&_kernel_start;
static uint64_t kernel_end_addr = (uint64_t)&_kernel_end;
static uint64_t text_end_addr = (uint64_t)&_text_end;
static uint64_t rodata_start_addr = (uint64_t)&_rodata_start;
static uint64_t rodata_end_addr = (uint64_t)&_rodata_end;
static uint64_t data_start_addr = (uint64_t)&_data_start;
// static uint64_t bss_start_addr =    (uint64_t)&_bss_start;

extern uint16_t mmap_entry_count;
extern mmap_entry_0xe820 mmap_bios_entries[];

// uint64_t* pml5t_phys_addr = NULL;
static uint64_t* kernel_pml4t = NULL;
static struct rb_node* kernel_rb_root = NULL;
// responsible for locking kernel pml4/vma tree modification
static spinlock_t kernel_page_lock;

static kmem_cache vma_cache;

inline uint64_t vma_start_of_node(struct rb_node* x) {
    const vma_t* v = container_of(x, struct vma, node);
    return directmap_p2v(v)->vma_start;
}

// rb_tree<vma_t> t;
static inline void rb_insert_vma(vma_t* v) {
    // builds a node for the vma and inserts it to the vma rb-tree
    // passes vma comparator and kernel vma rb tree root
    rb_insert(&kernel_rb_root, &v->node, vma_cmp);
}

static inline void rb_remove_vma(vma_t* v) {
    rb_delete(&kernel_rb_root, &v->node, vma_cmp);
}

// only use kmap_page/kmap_huge_page for mappings that literally ARE 1 page mappings
// this is because every call to those functions, inserts a VMA node
void kmap_page(const void* paddr, const void* vaddr, const uint64_t flags) {
    vma_t* vma = kmem_cache_kalloc(&vma_cache);
    if (vma == NULL)
        PANIC("FAILED TO ALLOCATE VMA STRUCT IN KMAP_PAGE\0")
    directmap_p2v(vma)->vma_start = (uint64_t)vaddr;
    directmap_p2v(vma)->vma_end = (uint64_t)vaddr + PAGE_SIZE;
    directmap_p2v(vma)->vma_pte = (uint64_t)paddr | flags;

    spinlock_acquire(&kernel_page_lock);
    rb_insert_vma(vma);
    map_page(kernel_pml4t, paddr, vaddr, flags);
    spinlock_release(&kernel_page_lock);
}

void kunmap_page(const void* paddr, const void* vaddr) {
    vma_t* vma = rb_find_vma(&kernel_rb_root, (uint64_t)vaddr);
    if (vma == NULL)
        return;

    spinlock_acquire(&kernel_page_lock);
    rb_remove_vma(vma);
    kmem_cache_kfree(&vma_cache, vma);
    unmap_page(kernel_pml4t, paddr, vaddr);
    spinlock_release(&kernel_page_lock);
}

void kmap_huge_page(const void* paddr, const void* vaddr,
                    const uint64_t flags) {
    vma_t* vma = kmem_cache_kalloc(&vma_cache);
    if (vma == NULL) {
        PANIC("FAILED TO ALLOCATE HUGE VMA STRUCT IN KMAP_PAGE\0")
    }
    directmap_p2v(vma)->vma_start = (uint64_t)vaddr;
    directmap_p2v(vma)->vma_end = (uint64_t)vaddr + HUGE_PAGE_SIZE;
    directmap_p2v(vma)->vma_pte = (uint64_t)paddr | flags;

    spinlock_acquire(&kernel_page_lock);
    rb_insert_vma(vma);
    map_huge_page(kernel_pml4t, paddr, vaddr, flags);
    spinlock_release(&kernel_page_lock);
}

void kunmap_huge_page(const void* paddr, const void* vaddr) {
    vma_t* vma = rb_find_vma(&kernel_rb_root, (uint64_t)vaddr);
    if (vma == NULL) {
        // never mapped
        return;
    }

    spinlock_acquire(&kernel_page_lock);
    rb_remove_vma(vma);
    kmem_cache_kfree(&vma_cache, vma);
    unmap_huge_page(kernel_pml4t, paddr, vaddr);
    spinlock_release(&kernel_page_lock);
}

void kmap_pages(const void* paddr, const void* vaddr, const uint64_t flags,
                const size_t num_pages) {
    if (flags & PTE_HUGE_PAGE) {
        // huge page mappings
        vma_t* vma = kmem_cache_kalloc(&vma_cache);
        if (vma == NULL) {
            PANIC("FAILED TO ALLOCATE A VMA STRUCT IN KMAP_PAGES\0");
        }
        directmap_p2v(vma)->vma_start = (uint64_t)vaddr;
        directmap_p2v(vma)->vma_end = (uint64_t)vaddr + num_pages * HUGE_PAGE_SIZE;
        directmap_p2v(vma)->vma_pte = (uint64_t)paddr | flags;

        spinlock_acquire(&kernel_page_lock);
        rb_insert_vma(vma);
    } else {
        // 4KB pages
        vma_t* vma = kmem_cache_kalloc(&vma_cache);
        if (vma == NULL) {
            PANIC("FAILED TO ALLOCATE A VMA STRUCT IN KMAP_PAGES\0");
        }
        directmap_p2v(vma)->vma_start = (uint64_t)vaddr;
        directmap_p2v(vma)->vma_end = (uint64_t)vaddr + num_pages * PAGE_SIZE;
        directmap_p2v(vma)->vma_pte = (uint64_t)paddr | flags;

        spinlock_acquire(&kernel_page_lock);
        rb_insert_vma(vma);
    }
    map_pages(kernel_pml4t, paddr, vaddr, flags, num_pages);
    spinlock_release(&kernel_page_lock);
}

void kunmap_pages(const void* paddr, const void* vaddr, const uint64_t flags,
                  const uint64_t num_pages) {
    vma_t* vma = rb_find_vma(&kernel_rb_root, (uint64_t)vaddr);
    if (vma == NULL) {
        // never mapped
        return;
    }

    spinlock_acquire(&kernel_page_lock);
    rb_remove_vma(vma);
    kmem_cache_kfree(&vma_cache, vma);
    unmap_pages(kernel_pml4t, paddr, vaddr, flags, num_pages);
    spinlock_release(&kernel_page_lock);
}


/* initialise kernel VM */
static inline void memset_pg(void* ptr) {
    uint8_t* p = ptr;
    for (uint64_t i = 0; i < 4096; i++) {
        p[i] = 0;
    }
}

static void map_framebuffer() {
    const boot_vbe_handoff* vbe_info = (boot_vbe_handoff*)(vbe_handoff_address);
    const uint64_t fb_last =
        (uint64_t)((uint64_t)vbe_info->framebuffer_address +
                   (uint32_t)vbe_info->pitch * vbe_info->height_px - 1);

    const uint64_t base_aligned = vbe_info->framebuffer_address & ~(PAGE_SIZE - 1);
    const uint64_t end_aligned = (fb_last + PAGE_SIZE) & ~(PAGE_SIZE - 1); // align up
    const uint64_t num_pages = (end_aligned - base_aligned) / PAGE_SIZE;

    kmap_pages((uint64_t*)base_aligned,
               (uint64_t*)((uint64_t)DIRECT_MAP_START + base_aligned),
               PTE_FLAGS_DIRECT_MAP | PTE_PWT, num_pages);
}

static uint64_t* map_stacks(void) {
    // returns virtual address to put in rsp
    // allocate first stack for current bootstrap processor
    uint64_t* phys_stack_addr = (uint64_t*)phys_kmalloc(
        4 * PAGE_SIZE, BUDDY_ALLOC); // allocate 16KB
    if (phys_stack_addr == NULL)
        PANIC("Failed to allocate 16KiB page for stack");

    for (uint32_t i = 0; i < 4; i++)
        memset_pg(directmap_p2v((char*)phys_stack_addr) + i * PAGE_SIZE);

    kmap_pages(phys_stack_addr, (void*)KERNEL_STACKS_START,
               PTE_FLAGS_KERNEL_DATA, 4);
    // stack permissions are like kernel data section

    return (uint64_t*)(KERNEL_STACKS_START + 4 * PAGE_SIZE - 8);
    // last 8-byte aligned slot in mapped region; after ret pops it, RSP lands
    // 16-byte aligned
}

static void map_direct_mapping(void) {
    // reads e820 map and maps usable memory
    for (uint32_t i = 0; i < mmap_entry_count; i++) {
        if (mmap_bios_entries[i].type != 0x01)
            continue;

        void* base_addr = mmap_bios_entries[i].base_address;
        const uint64_t size = mmap_bios_entries[i].chunk_size;
        const void* last_addr = (void*)((uint64_t)base_addr + size);

        // align UP physical address to 4KB
        const uint64_t base_addr_aligned =
            ((uint64_t)base_addr + PAGE_SIZE - 1) & ~((uint64_t)PAGE_SIZE - 1);
        // align DOWN last_addr
        const uint64_t last_addr_aligned =
            (uint64_t)last_addr & ~((uint64_t)PAGE_SIZE - 1);

        if (base_addr_aligned >= last_addr_aligned)
            continue; // nothing to map

        // 2MB aligned span carved out of the region for huge page mappings
        const uint64_t huge_base = (base_addr_aligned + HUGE_PAGE_SIZE - 1) &
                                   ~(HUGE_PAGE_SIZE - 1); // align UP
        const uint64_t huge_end =
            last_addr_aligned & ~(HUGE_PAGE_SIZE - 1); // ALIGN DOWN

        if (huge_base >= huge_end) {
            // whole region fits in 4KB pages
            const uint64_t pte_num =
                (last_addr_aligned - base_addr_aligned) / PAGE_SIZE;
            kmap_pages((void*)base_addr_aligned,
                       (void*)(DIRECT_MAP_START + base_addr_aligned),
                       PTE_FLAGS_DIRECT_MAP, pte_num);
        } else {
            // head 4KB pages up to the first 2MB boundary
            if (base_addr_aligned < huge_base) {
                const uint64_t pre_pte_num =
                    (huge_base - base_addr_aligned) / PAGE_SIZE;
                kmap_pages((void*)base_addr_aligned,
                           (void*)(DIRECT_MAP_START + base_addr_aligned),
                           PTE_FLAGS_DIRECT_MAP, pre_pte_num);
            }

            // 2MB huge pages for the aligned middle span
            const uint64_t huge_count = (huge_end - huge_base) / HUGE_PAGE_SIZE;
            kmap_pages((void*)huge_base, (void*)(DIRECT_MAP_START + huge_base),
                       PTE_FLAGS_DIRECT_MAP | PTE_HUGE_PAGE, huge_count);

            // tail 4KB pages past the last 2MB boundary
            const uint64_t leftover_pte_num =
                (last_addr_aligned - huge_end) / PAGE_SIZE;
            if (leftover_pte_num > 0) {
                kmap_pages((void*)huge_end,
                           (void*)(DIRECT_MAP_START + huge_end),
                           PTE_FLAGS_DIRECT_MAP, leftover_pte_num);
            }
        }
    }
}

static inline void map_boot_memory(void) {
    // Important!
    kmap_pages(0, 0, 0ULL | PTE_PRESENT | PTE_WRITABLE | PTE_HUGE_PAGE, 8);
}


void unmap_boot_memory() {
    kunmap_pages(0, 0, 0ULL | PTE_PRESENT | PTE_WRITABLE | PTE_HUGE_PAGE, 8);
}


uint64_t* __init_mmu(void) {
    spinlock_init(&kernel_page_lock);

    // initialise kernel rb-tree and vma cache
    kmem_cache_create_sl(&vma_cache, sizeof(vma_t));

    kernel_pml4t = (uint64_t*)phys_kmalloc(PAGE_SIZE, BUDDY_ALLOC);
    if (kernel_pml4t == NULL) {
        PANIC("FAILED TO ALLOCATE PML4T ROOT FOR THE KERNEL\0");
    }
    memset_pg(directmap_p2v(kernel_pml4t));

    uint64_t vaddr = kernel_start_addr;

    // map (_kernel_start to _text_end) to virtual address: KERNEL_IMAGE_START
    const uint64_t text_section_size = text_end_addr - kernel_start_addr;
    uint64_t num_pages =
        text_section_size /
        PAGE_SIZE; // text_end and kernel_start should be 4KB aligned

    kmap_pages(
        (void*)kernel_start_addr - KERNEL_IMAGE_START,
        (void*)vaddr,
        PTE_FLAGS_KERNEL_CODE,
        num_pages);

    // map (rodata start to rodata end) to
    // KERNEL_BASED_PHYS_TO_VIRT(_rodata_start)

    const uint64_t rodata_section_size = rodata_end_addr - rodata_start_addr;
    num_pages = rodata_section_size / PAGE_SIZE;

    vaddr = rodata_start_addr;
    // vaddr = KERNEL_BASED_PHYS_TO_VIRT(rodata_start_addr);

    kmap_pages(
        (void*)rodata_start_addr - KERNEL_IMAGE_START,
        (void*)vaddr,
        0ULL | PTE_PRESENT | PTE_NX | PTE_GLOBAL,
        num_pages);

    // map (_data_start to _kernel_end) to
    // KERNEL_BASED_PHYS_TO_VIRT(_data_start)

    const uint64_t data_section_size = kernel_end_addr - data_start_addr;
    num_pages = data_section_size / PAGE_SIZE;

    vaddr = data_start_addr;
    // vaddr = KERNEL_BASED_PHYS_TO_VIRT(data_start_addr);

    kmap_pages(
        (void*)data_start_addr - KERNEL_IMAGE_START,
        (void*)vaddr,
        PTE_FLAGS_KERNEL_DATA,
        num_pages);

    // map buddy metadata
    // lives after kernel end
    // map 16kb after the kernel
    vaddr += num_pages * PAGE_SIZE;
    const void* pa = (void*)((uint64_t)vaddr - KERNEL_IMAGE_START);

    kmap_pages(
        pa,
        (void*)vaddr,
        PTE_PRESENT | PTE_NX | PTE_GLOBAL | PTE_WRITABLE,
        4);

    uint64_t* virt_rsp = map_stacks();
    // map stack
    map_direct_mapping();
    map_boot_memory();

    map_framebuffer();
    // add VBE framebuffer to direct map

    load_pmlt_cr3(kernel_pml4t); // mov cr3, pml4t
                                 // bootloader pml4t is discarded in here

    // From here on the bootloader's 4GB identity map is gone, so a raw physical
    // address is no longer a usable pointer. Everything the allocators handed
    // out is reached through the direct map instead; stored values are
    // untouched, only dereferences translate. Must come after the CR3 load:
    // the direct map only exists in the table we just installed.
    phys_map_offset = DIRECT_MAP_START;

    const boot_vbe_handoff* vbe_info_local =
        (const boot_vbe_handoff*)(vbe_handoff_address);
    vesa_set_fb_virtual(DIRECT_MAP_START + vbe_info_local->framebuffer_address);

    return virt_rsp;
}
