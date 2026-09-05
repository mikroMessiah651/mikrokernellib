/* Eyal Kaghanovich
 * responsible for initialising the first memory layout for the kernel,
 * jumping to virtual memory-mapped address of kernel entry
 */

#include "include/mmu.h"
#include "include/kstrings.h"
#include "include/mikroKernellib-common.h"
#include "include/phys_kmalloc.h"
#include <stdbool.h>
#include <stdint.h>


inline void invlpg(const void* addr) {
    asm volatile("invlpg (%0)" : : "r"(addr) : "memory");
}

static inline void* memset_pg(void* ptr) {
    return memset(ptr, 0, PAGE_SIZE);
}

static inline bool is_empty(const uint64_t* page_directory) {
    // returns true if the page directory is empty
    // an empty page directory means a zeroed page
    for (uint32_t i = 0; i < 512; i++) {
        if (page_directory[i] != 0ULL) {
            return false;
        }
    }
    return true;
}

/* The single definition. Declared extern in mmu.h, forced into .data because
 * .bss is never loaded or zeroed on this kernel. Flipped to DIRECT_MAP_START
 * by __init_mmu() right after load_pmlt_cr3(). */
uint64_t phys_map_offset __attribute__((section(".data"))) = 0;

// ARE WE TOUCHING PTEs WITH NO LOCK????!!!
// leave lock to a higher abstraction layer
// amazing vuln-less C code design btw

void map_page(uint64_t* pml4t, const void* paddr, const void* vaddr,
              const uint64_t flags) {
    if (flags & PTE_HUGE_PAGE)
        map_huge_page(pml4t, paddr, vaddr, flags);
    return;
    // PANIC("INCORRECTLY MAPPED HUGE PAGE WITH REGULAR MAP PAGE FUNCTION\0");

    // align down to 4KB
    const uint64_t aligned_addr = (uint64_t)paddr & ~((uint64_t)PAGE_SIZE - 1);
    const uint64_t pte = aligned_addr | flags;

    const uint64_t pml4t_idx = VIRT_TO_PML4_IDX((uint64_t)vaddr);
    const uint64_t pdpt_idx = VIRT_TO_PDPT_IDX((uint64_t)vaddr);
    const uint64_t pd_idx = VIRT_TO_PD_IDX((uint64_t)vaddr);
    const uint64_t pt_idx = VIRT_TO_PT_IDX((uint64_t)vaddr);

    // wire pml4t...
    if (!(directmap_p2v_deref(pml4t + pml4t_idx) & PTE_PRESENT)) {
        uint64_t* new_pdpt = (uint64_t*)phys_kmalloc(4096, BUDDY_ALLOC);
        if (new_pdpt == NULL)
            PANIC("Failed to allocate 4KiB page for PDPT\0");
        memset_pg(directmap_p2v(new_pdpt));

        directmap_p2v_deref(pml4t + pml4t_idx) = (uint64_t)new_pdpt | PTE_PRESENT | PTE_WRITABLE;
    }
    uint64_t* pdpt = (uint64_t*)(directmap_p2v_deref(pml4t + pml4t_idx) & PTE_ADDR_MASK);

    if (!(directmap_p2v_deref(pdpt + pdpt_idx) & PTE_PRESENT)) {
        uint64_t* new_pd = (uint64_t*)phys_kmalloc(4096, BUDDY_ALLOC);
        if (new_pd == NULL)
            PANIC("Failed to allocate 4KiB page for PD\0");
        memset_pg(directmap_p2v(new_pd));

        directmap_p2v_deref(pdpt + pdpt_idx) = (uint64_t)new_pd | PTE_PRESENT | PTE_WRITABLE;
    }
    uint64_t* pd = (uint64_t*)(directmap_p2v_deref(pdpt + pdpt_idx) & PTE_ADDR_MASK);

    if (!(directmap_p2v_deref(pd + pd_idx) & PTE_PRESENT)) {
        uint64_t* new_pt = (uint64_t*)phys_kmalloc(4096, BUDDY_ALLOC);
        if (new_pt == NULL)
            PANIC("Failed to allocate 4KiB page for PT\0");
        memset_pg(directmap_p2v(new_pt));

        directmap_p2v_deref(pd + pd_idx) = (uint64_t)new_pt | PTE_PRESENT | PTE_WRITABLE;
    }
    uint64_t* pt = (uint64_t*)(directmap_p2v_deref(pd + pd_idx) & PTE_ADDR_MASK);

    directmap_p2v_deref(pt + pt_idx) = pte;
}

void unmap_page(const uint64_t* pml4t, const void* paddr, const void* vaddr) {
    // 4KB aligned
    const uint64_t aligned_paddr = (uint64_t)paddr & ~((uint64_t)PAGE_SIZE - 1);

    const uint64_t pml4t_idx = VIRT_TO_PML4_IDX((uint64_t)vaddr);
    const uint64_t pdpt_idx = VIRT_TO_PDPT_IDX((uint64_t)vaddr);
    const uint64_t pd_idx = VIRT_TO_PD_IDX((uint64_t)vaddr);
    const uint64_t pt_idx = VIRT_TO_PT_IDX((uint64_t)vaddr);

    // check page table entries
    if (!(directmap_p2v_deref(pml4t + pml4t_idx) & PTE_PRESENT)) {
        // page wasn't mapped
        return;
    }
    uint64_t* pdpt = (uint64_t*)(directmap_p2v_deref(pml4t + pml4t_idx) & PTE_ADDR_MASK);

    if (!(directmap_p2v_deref(pdpt + pdpt_idx) & PTE_PRESENT)) {
        return;
    }
    uint64_t* pd = (uint64_t*)(directmap_p2v_deref(pdpt + pdpt_idx) & PTE_ADDR_MASK);

    if (!(directmap_p2v_deref(pd + pd_idx) & PTE_PRESENT)) {
        return;
    } else if (directmap_p2v_deref(pd + pd_idx) & PTE_HUGE_PAGE) {
        // unmapped huge page with unmap regular page function -> PANIC
        PANIC("UNMAPPED HUGE PAGE WITH REGULAR UNMAP_PAGE FUNCTION\0");
    }
    uint64_t* pt = (uint64_t*)(directmap_p2v_deref(pd + pd_idx) & PTE_ADDR_MASK);

    if (!(directmap_p2v_deref(pt + pt_idx) & PTE_PRESENT)) {
        return;
    }

    if ((directmap_p2v_deref(pt + pt_idx) & PTE_ADDR_MASK) != aligned_paddr) {
        // wrong page
        PANIC("TRIED TO UNMAP WRONG PAGE");
    }
    // page is mapped
    // zero the entry
    directmap_p2v_deref(pt + pt_idx) = 0ULL;
    invlpg(vaddr);

    // check if pt is empty
    if (is_empty(directmap_p2v(pt))) {
        directmap_p2v_deref(pd + pd_idx) = 0ULL;
        phys_kfree(pt, PAGE_SIZE, BUDDY_ALLOC);
        // check the pd
        if (is_empty(directmap_p2v(pd))) {
            directmap_p2v_deref(pdpt + pdpt_idx) = 0ULL;
            phys_kfree(pd, PAGE_SIZE, BUDDY_ALLOC);
            // check the pdpt
            if (is_empty(directmap_p2v(pdpt))) {
                directmap_p2v_deref(pml4t + pml4t_idx) = 0ULL;
                phys_kfree(pdpt, PAGE_SIZE, BUDDY_ALLOC);
            }
        }
    }
}

void map_huge_page(uint64_t* pml4t, const void* paddr, const void* vaddr,
                   const uint64_t flags) {
    if (!(flags & PTE_HUGE_PAGE))
        PANIC("INCORRECTLY MAPPED PAGE WITH MAP HUGE PAGE FUNCTION\0");

    // get indices
    const uint64_t pml4t_idx = VIRT_TO_PML4_IDX((uint64_t)vaddr);
    const uint64_t pdpt_idx = VIRT_TO_PDPT_IDX((uint64_t)vaddr);
    uint64_t pd_idx = VIRT_TO_PD_IDX((uint64_t)vaddr);

    // walk the page tables
    if (!(directmap_p2v_deref(pml4t + pml4t_idx) & PTE_PRESENT)) {
        uint64_t* new_pdpt = (uint64_t*)phys_kmalloc(4096, BUDDY_ALLOC);
        if (new_pdpt == NULL)
            PANIC("Failed to allocate 4KiB page for PDPT\0");
        memset_pg(directmap_p2v(new_pdpt));

        directmap_p2v_deref(pml4t + pml4t_idx) = (uint64_t)new_pdpt | PTE_PRESENT | PTE_WRITABLE;
    }
    uint64_t* pdpt = (uint64_t*)(directmap_p2v_deref(pml4t + pml4t_idx) & PTE_ADDR_MASK);

    if (!(directmap_p2v_deref(pdpt + pdpt_idx) & PTE_PRESENT)) {
        uint64_t* new_pd = (uint64_t*)phys_kmalloc(4096, BUDDY_ALLOC);
        if (new_pd == NULL)
            PANIC("Failed to allocate 4KiB page for PD\0");
        memset_pg(directmap_p2v(new_pd));

        directmap_p2v_deref(pdpt + pdpt_idx) = (uint64_t)new_pd | PTE_PRESENT | PTE_WRITABLE;
    }
    uint64_t* pd = (uint64_t*)(directmap_p2v_deref(pdpt + pdpt_idx) & PTE_ADDR_MASK);

    // create the mapping
    directmap_p2v_deref(pd + pd_idx) = ((uint64_t)paddr) | flags;
}

void unmap_huge_page(uint64_t* pml4t, const void* paddr, const void* vaddr) {
    const uint64_t aligned_paddr =
        (uint64_t)paddr & ~((uint64_t)HUGE_PAGE_SIZE - 1);

    const uint64_t pml4t_idx = VIRT_TO_PML4_IDX((uint64_t)vaddr);
    const uint64_t pdpt_idx = VIRT_TO_PDPT_IDX((uint64_t)vaddr);
    const uint64_t pd_idx = VIRT_TO_PD_IDX((uint64_t)vaddr);

    if (!(directmap_p2v_deref(pml4t + pml4t_idx) & PTE_PRESENT)) {
        return;
    }
    uint64_t* pdpt = (uint64_t*)(directmap_p2v_deref(pml4t + pml4t_idx) & PTE_ADDR_MASK);

    if (!(directmap_p2v_deref(pdpt + pdpt_idx) & PTE_PRESENT)) {
        return;
    }
    uint64_t* pd = (uint64_t*)(directmap_p2v_deref(pdpt + pdpt_idx) & PTE_ADDR_MASK);

    if (!(directmap_p2v_deref(pd + pd_idx) & PTE_PRESENT))
        return; // page wasnt mapped, should i invlpg?
    if (!(directmap_p2v_deref(pd + pd_idx) & PTE_HUGE_PAGE))
        PANIC("TRIED TO UNMAP A REGULAR PAGE WITH UNMAP_HUGE_PAGE FUNCTION\0");

    if ((directmap_p2v_deref(pd + pd_idx) & PTE_ADDR_MASK) != aligned_paddr) {
        PANIC("TRIED TO UNMAP WRONG HUGE PAGE\0");
    }

    // zero the pte
    directmap_p2v_deref(pd + pd_idx) = 0ULL;
    invlpg(vaddr);

    // check if the pd is empty
    if (is_empty(directmap_p2v(pd))) {
        directmap_p2v_deref(pdpt + pdpt_idx) = 0ULL;
        phys_kfree(pd, PAGE_SIZE, BUDDY_ALLOC);
        // check pdpt
        if (is_empty(directmap_p2v(pdpt))) {
            directmap_p2v_deref(pml4t + pml4t_idx) = 0ULL;
            phys_kfree(pdpt, PAGE_SIZE, BUDDY_ALLOC);
        }
    }
}

void map_pages(uint64_t* pml4t, const void* paddr, const void* vaddr,
               const uint64_t flags, const uint64_t num_pages) {
    if (num_pages == 0)
        return;

    if (flags & PTE_HUGE_PAGE) {
        uint64_t cur_vaddr = (uint64_t)vaddr;
        uint64_t cur_paddr = (uint64_t)paddr;

        uint64_t cur_pml4t_idx = ~0ULL;
        uint64_t cur_pdpt_idx = ~0ULL;
        uint64_t* pdpt = NULL;
        uint64_t* pd = NULL;

        for (uint64_t count = 0; count < num_pages; count++) {
            uint64_t new_pml4t_idx = VIRT_TO_PML4_IDX(cur_vaddr);
            uint64_t new_pdpt_idx = VIRT_TO_PDPT_IDX(cur_vaddr);

            if (new_pml4t_idx != cur_pml4t_idx) {
                cur_pml4t_idx = new_pml4t_idx;
                if (!(directmap_p2v_deref(pml4t + cur_pml4t_idx) & PTE_PRESENT)) {
                    uint64_t* new_pdpt =
                        (uint64_t*)phys_kmalloc(4096, BUDDY_ALLOC);
                    if (new_pdpt == NULL)
                        PANIC("Failed to allocate 4KiB page for PDPT\0");
                    memset_pg(directmap_p2v(new_pdpt));
                    directmap_p2v_deref(pml4t + cur_pml4t_idx) =
                        (uint64_t)new_pdpt | PTE_PRESENT | PTE_WRITABLE;
                }
                pdpt = (uint64_t*)(directmap_p2v_deref(pml4t + cur_pml4t_idx) & PTE_ADDR_MASK);
                cur_pdpt_idx = ~0ULL;
            }

            if (new_pdpt_idx != cur_pdpt_idx) {
                cur_pdpt_idx = new_pdpt_idx;
                if (!(directmap_p2v_deref(pdpt + cur_pdpt_idx) & PTE_PRESENT)) {
                    uint64_t* new_pd =
                        (uint64_t*)phys_kmalloc(4096, BUDDY_ALLOC);
                    if (new_pd == NULL)
                        PANIC("Failed to allocate 4KiB page for PD\0");
                    memset_pg(directmap_p2v(new_pd));
                    directmap_p2v_deref(pdpt + cur_pdpt_idx) =
                        (uint64_t)new_pd | PTE_PRESENT | PTE_WRITABLE;
                }
                pd = (uint64_t*)(directmap_p2v_deref(pdpt + cur_pdpt_idx) & PTE_ADDR_MASK);
            }

            directmap_p2v_deref(pd + VIRT_TO_PD_IDX(cur_vaddr)) = cur_paddr | flags;
            cur_vaddr += HUGE_PAGE_SIZE;
            cur_paddr += HUGE_PAGE_SIZE;
        }
        return;
    }

    // 4KB pages
    uint64_t cur_vaddr = (uint64_t)vaddr;
    uint64_t cur_paddr = (uint64_t)paddr & ~((uint64_t)PAGE_SIZE - 1);

    uint64_t cur_pml4t_idx = ~0ULL;
    uint64_t cur_pdpt_idx = ~0ULL;
    uint64_t cur_pd_idx = ~0ULL;
    uint64_t* pdpt = NULL;
    uint64_t* pd = NULL;
    uint64_t* pt = NULL;

    for (uint64_t count = 0; count < num_pages; count++) {
        uint64_t new_pml4t_idx = VIRT_TO_PML4_IDX(cur_vaddr);
        uint64_t new_pdpt_idx = VIRT_TO_PDPT_IDX(cur_vaddr);
        uint64_t new_pd_idx = VIRT_TO_PD_IDX(cur_vaddr);

        if (new_pml4t_idx != cur_pml4t_idx) {
            cur_pml4t_idx = new_pml4t_idx;
            if (!(directmap_p2v_deref(pml4t + cur_pml4t_idx) & PTE_PRESENT)) {
                uint64_t* new_pdpt =
                    (uint64_t*)phys_kmalloc(4096, BUDDY_ALLOC);
                if (new_pdpt == NULL)
                    PANIC("Failed to allocate 4KiB page for PDPT\0");
                memset_pg(directmap_p2v(new_pdpt));

                directmap_p2v_deref(pml4t + cur_pml4t_idx) =
                    (uint64_t)new_pdpt | PTE_PRESENT | PTE_WRITABLE;
            }
            pdpt = (uint64_t*)(directmap_p2v_deref(pml4t + cur_pml4t_idx) & PTE_ADDR_MASK);
            cur_pdpt_idx = ~0ULL;
        }

        if (new_pdpt_idx != cur_pdpt_idx) {
            cur_pdpt_idx = new_pdpt_idx;
            if (!(directmap_p2v_deref(pdpt + cur_pdpt_idx) & PTE_PRESENT)) {
                uint64_t* new_pd =
                    (uint64_t*)phys_kmalloc(4096, BUDDY_ALLOC);
                if (new_pd == NULL)
                    PANIC("Failed to allocate 4KiB page for PD\0");
                memset_pg(directmap_p2v(new_pd));

                directmap_p2v_deref(pdpt + cur_pdpt_idx) =
                    (uint64_t)new_pd | PTE_PRESENT | PTE_WRITABLE;
            }
            pd = (uint64_t*)(directmap_p2v_deref(pdpt + cur_pdpt_idx) & PTE_ADDR_MASK);
            cur_pd_idx = ~0ULL;
        }

        if (new_pd_idx != cur_pd_idx) {
            cur_pd_idx = new_pd_idx;
            if (!(directmap_p2v_deref(pd + cur_pd_idx) & PTE_PRESENT)) {
                uint64_t* new_pt =
                    (uint64_t*)phys_kmalloc(4096, BUDDY_ALLOC);
                if (new_pt == NULL)
                    PANIC("Failed to allocate 4KiB page for PT\0");
                memset_pg(directmap_p2v(new_pt));

                directmap_p2v_deref(pd + cur_pd_idx) = (uint64_t)new_pt | PTE_PRESENT | PTE_WRITABLE;
            }
            pt = (uint64_t*)(directmap_p2v_deref(pd + cur_pd_idx) & PTE_ADDR_MASK);
        }

        directmap_p2v_deref(pt + VIRT_TO_PT_IDX(cur_vaddr)) = cur_paddr | flags;
        cur_vaddr += PAGE_SIZE;
        cur_paddr += PAGE_SIZE;
    }
}

void unmap_pages(uint64_t* pml4t, const void* paddr, const void* vaddr,
                 const uint64_t flags, const uint64_t num_pages) {
    if (num_pages <= 0)
        return;

    if (flags & PTE_HUGE_PAGE) {
        // zero num_pages PTEs based on vaddr, sanity check paddr once
        // calculate based on vaddr and num_pages the current vaddr to extract
        // indices from align paddr
        paddr = (uint64_t*)((uint64_t)paddr & ~((uint64_t)HUGE_PAGE_SIZE - 1));

        for (uint64_t i = 0; i < num_pages; i++) {
            const uint64_t* curr_vaddr =
                (uint64_t*)((uint64_t)vaddr + i * HUGE_PAGE_SIZE);
            const uint64_t* curr_aligned_paddr =
                (uint64_t*)((uint64_t)paddr + i * HUGE_PAGE_SIZE);

            const uint64_t pml4t_idx = VIRT_TO_PML4_IDX((uint64_t)curr_vaddr);
            const uint64_t pdpt_idx = VIRT_TO_PDPT_IDX((uint64_t)curr_vaddr);
            const uint64_t pd_idx = VIRT_TO_PD_IDX((uint64_t)curr_vaddr);

            if (!(directmap_p2v_deref(pml4t + pml4t_idx) & PTE_PRESENT))
                continue;
            // if a page is unmapped then we just continue, unless its the last
            // page in which case we return

            uint64_t* pdpt = (uint64_t*)(directmap_p2v_deref(pml4t + pml4t_idx) & PTE_ADDR_MASK);
            if (!(directmap_p2v_deref(pdpt + pdpt_idx) & PTE_PRESENT))
                continue;

            uint64_t* pd = (uint64_t*)(directmap_p2v_deref(pdpt + pdpt_idx) & PTE_ADDR_MASK);
            if (!(directmap_p2v_deref(pd + pd_idx) & PTE_PRESENT))
                continue;

            if (!(directmap_p2v_deref(pd + pd_idx) & PTE_HUGE_PAGE)) {
                // if tried to unmap a regular page with incorrect flags, PANIC
                PANIC("TRIED TO UNMAP A REGULAR PAGE WITH INCORRECT FLAGS "
                      "ARGUMENT\0");
            }
            // sanity check if its the correct mapping
            if ((directmap_p2v_deref(pd + pd_idx) & PTE_ADDR_MASK) != (uint64_t)curr_aligned_paddr) {
                PANIC("TRIED TO UNMAP WRONG HUGE PAGES\0");
            }

            // zero the pte, invlpg
            directmap_p2v_deref(pd + pd_idx) = 0ULL;
            invlpg(curr_vaddr);

            // check if the pd should be freed
            if (is_empty(directmap_p2v(pd))) {
                directmap_p2v_deref(pdpt + pdpt_idx) = 0ULL;
                phys_kfree(pd, PAGE_SIZE, BUDDY_ALLOC);
                // check pdpt
                if (is_empty(directmap_p2v(pdpt))) {
                    directmap_p2v_deref(pml4t + pml4t_idx) = 0ULL;
                    phys_kfree(pdpt, PAGE_SIZE, BUDDY_ALLOC);
                }
            }
        }
        return;
    }

    // 4KB pages
    // align paddr
    paddr = (uint64_t*)((uint64_t)paddr & ~((uint64_t)PAGE_SIZE - 1));

    for (uint64_t i = 0; i < num_pages; i++) {
        const uint64_t* curr_vaddr =
            (uint64_t*)((uint64_t)vaddr + i * PAGE_SIZE);
        const uint64_t* curr_aligned_paddr =
            (uint64_t*)((uint64_t)paddr + i * PAGE_SIZE);

        const uint64_t pml4t_idx = VIRT_TO_PML4_IDX((uint64_t)curr_vaddr);
        const uint64_t pdpt_idx = VIRT_TO_PDPT_IDX((uint64_t)curr_vaddr);
        const uint64_t pd_idx = VIRT_TO_PD_IDX((uint64_t)curr_vaddr);
        const uint64_t pt_idx = VIRT_TO_PT_IDX((uint64_t)curr_vaddr);

        if (!(directmap_p2v_deref(pml4t + pml4t_idx) & PTE_PRESENT))
            continue;
        // if a page is unmapped then we just continue

        uint64_t* pdpt = (uint64_t*)(directmap_p2v_deref(pml4t + pml4t_idx) & PTE_ADDR_MASK);
        if (!(directmap_p2v_deref(pdpt + pdpt_idx) & PTE_PRESENT))
            continue;

        uint64_t* pd = (uint64_t*)(directmap_p2v_deref(pdpt + pdpt_idx) & PTE_ADDR_MASK);
        if (!(directmap_p2v_deref(pd + pd_idx) & PTE_PRESENT))
            continue;

        // check if page is huge
        if (directmap_p2v_deref(pd + pd_idx) & PTE_HUGE_PAGE) {
            PANIC("TRIED TO UNMAP HUGE PAGE WITH REGULAR PAGE FLAGS IN "
                  "UNMAP_PAGES\0");
        }

        uint64_t* pt = (uint64_t*)(directmap_p2v_deref(pd + pd_idx) & PTE_ADDR_MASK);
        if (!(directmap_p2v_deref(pt + pt_idx) & PTE_PRESENT))
            continue;

        // sanity check if its the correct mapping
        if ((directmap_p2v_deref(pt + pt_idx) & PTE_ADDR_MASK) != (uint64_t)curr_aligned_paddr) {
            PANIC("TRIED TO UNMAP WRONG PAGES\0");
        }

        // zero the pte, invlpg
        directmap_p2v_deref(pt + pt_idx) = 0ULL;
        invlpg(curr_vaddr);

        // check if pt needs to be freed
        if (is_empty(directmap_p2v(pt))) {
            // free the pt
            directmap_p2v_deref(pd + pd_idx) = 0ULL;
            phys_kfree(pt, PAGE_SIZE, BUDDY_ALLOC);
            // check the pd
            if (is_empty(directmap_p2v(pd))) {
                directmap_p2v_deref(pdpt + pdpt_idx) = 0ULL;
                phys_kfree(pd, PAGE_SIZE, BUDDY_ALLOC);
                // check pdpt
                if (is_empty(directmap_p2v(pdpt))) {
                    directmap_p2v_deref(pml4t + pml4t_idx) = 0ULL;
                    phys_kfree(pdpt, PAGE_SIZE, BUDDY_ALLOC);
                }
            }
        }
    }
}
