/* Eyal Kaghnovich
 * responsible for initialising the first memory layout for the kernel,
 * jumping to virtual memory-mapped address of kernel entry
 * initialising virt_kmalloc
 *
 * needs spinlocks?
*/


#include "include/mikroKernellib-common.h"
#include "include/mmu_page_tables.h"
#include "include/vesa_graphics_lib.h"
#include "include/phys_kmalloc.h"
#include <stdbool.h>
#include <stdint.h>

uint8_t paging_levels = 4;

extern uint64_t _kernel_start;
extern uint64_t _kernel_end;
extern uint64_t _text_end;
extern uint64_t _rodata_start;
extern uint64_t _rodata_end;
extern uint64_t _data_start;
extern uint64_t _bss_start;

uint64_t kernel_start_addr = (uint64_t)&_kernel_start;
uint64_t kernel_end_addr = (uint64_t)&_kernel_end;
uint64_t text_end_addr = (uint64_t)&_text_end;
uint64_t rodata_start_addr = (uint64_t)&_rodata_start;
uint64_t rodata_end_addr = (uint64_t)&_rodata_end;
uint64_t data_start_addr = (uint64_t)&_data_start;
uint64_t bss_start_addr = (uint64_t)&_bss_start;

extern uint16_t mmap_entry_count;
extern mmap_entry_0xe820 mmap_bios_entries[];

uint64_t* pml5t_phys_addr = NULL;
uint64_t* pml4t_phys_addr = NULL;

uint64_t* virt_rsp = NULL;

static uint64_t* __init_mmu_page_tables_pml4(void);

static void memset_pg(void* ptr) {
    uint8_t* p = (uint8_t*)ptr;
    for (uint64_t i = 0; i < 4096; i++) {
        p[i] = 0;
    }
}

uint64_t* __init_mmu_paging() {
    return __init_mmu_page_tables_pml4();
}

static void map_vbe_fb() {
    const boot_vbe_handoff* vbe_info = (boot_vbe_handoff*)(vbe_handoff_address);
    const uint64_t fb_last_address = (uint64_t)((uint64_t)vbe_info->framebuffer_address + (uint32_t)vbe_info->pitch * vbe_info->height_px - 1);

    const uint64_t pml4t_idx = VIRT_TO_PML4_IDX(DIRECT_MAP_START + vbe_info->framebuffer_address);
    const uint64_t pdpt_idx = VIRT_TO_PDPT_IDX(DIRECT_MAP_START + vbe_info->framebuffer_address);
    const uint64_t pd_idx = VIRT_TO_PD_IDX(DIRECT_MAP_START + vbe_info->framebuffer_address);
    uint64_t pt_idx = VIRT_TO_PT_IDX(DIRECT_MAP_START + vbe_info->framebuffer_address);


    // wire pml to pt
    if (!(pml4t_phys_addr[pml4t_idx] & PTE_PRESENT)) {
        uint64_t* new_pdpt = (uint64_t*)phys_kmalloc(4096, PAGEFRAME_ALLOC);
        if (new_pdpt == NULL) PANIC("Failed to allocate 4KiB page for PDPT");
        memset_pg(new_pdpt);

        pml4t_phys_addr[pml4t_idx] = (uint64_t)new_pdpt | PTE_PRESENT | PTE_WRITABLE;
    }
    uint64_t* pdpt = (uint64_t*)(pml4t_phys_addr[pml4t_idx] & PTE_ADDR_MASK);

    if (!(pdpt[pdpt_idx] & PTE_PRESENT)) {
        uint64_t* new_pd = (uint64_t*)phys_kmalloc(4096, PAGEFRAME_ALLOC);
        if (new_pd == NULL) PANIC("Failed to allocate 4KiB page for PD");
        memset_pg(new_pd);

        pdpt[pdpt_idx] = (uint64_t)new_pd | PTE_PRESENT | PTE_WRITABLE;
    }
    uint64_t* pd = (uint64_t*)(pdpt[pdpt_idx] & PTE_ADDR_MASK);

    uint64_t cur_pd_idx = pd_idx;

    if (!(pd[cur_pd_idx] & PTE_PRESENT)) {
        uint64_t* new_pt = (uint64_t*)phys_kmalloc(4096, PAGEFRAME_ALLOC);
        if (new_pt == NULL) PANIC("Failed to allocate 4KiB page for PT");
        memset_pg(new_pt);

        pd[cur_pd_idx] = (uint64_t)new_pt | PTE_PRESENT | PTE_WRITABLE;
    }
    uint64_t* pt = (uint64_t*)(pd[cur_pd_idx] & PTE_ADDR_MASK);

    // map using 4KB pages; allocate a new PT whenever pt_idx overflows 512
    uint64_t fb_addr = (uint64_t)vbe_info->framebuffer_address;

    while (fb_addr <= fb_last_address) {
        if (pt_idx == 512) {
            cur_pd_idx++;
            uint64_t* new_pt = (uint64_t*)phys_kmalloc(4096, PAGEFRAME_ALLOC);
            if (new_pt == NULL) PANIC("Failed to allocate 4KiB page for PT (fb overflow)");
            memset_pg(new_pt);
            pd[cur_pd_idx] = (uint64_t)new_pt | PTE_PRESENT | PTE_WRITABLE;
            pt = new_pt;
            pt_idx = 0;
        }
        pt[pt_idx] = (uint64_t)(fb_addr | PTE_FLAGS_DIRECT_MAP | PTE_PWT);
        pt_idx++;
        fb_addr += PAGE_SIZE;
    }
}

static uint64_t* map_stacks_region(void) {
    // returns virtual address to put in rsp
    const uint64_t pml4t_idx = VIRT_TO_PML4_IDX(KERNEL_STACKS_START);
    const uint64_t pdpt_idx = VIRT_TO_PDPT_IDX(KERNEL_STACKS_START);
    const uint64_t pd_idx = VIRT_TO_PD_IDX(KERNEL_STACKS_START);
    uint64_t pt_idx = VIRT_TO_PT_IDX(KERNEL_STACKS_START);

    // allocate first stack for current bootstrap processor
    uint64_t* phys_stack_addr = (uint64_t*)phys_kmalloc(4 * PAGE_SIZE, PAGEFRAME_ALLOC); // allocate 16KB
    if (phys_stack_addr == NULL) PANIC("Failed to allocate 16KiB page for stack");

    for (uint32_t i = 0; i < 4; i++)
        memset_pg((char*)phys_stack_addr + i * PAGE_SIZE);

    // wire pml to pt
    if (!(pml4t_phys_addr[pml4t_idx] & PTE_PRESENT)) {
        uint64_t* new_pdpt = (uint64_t*)phys_kmalloc(4096, PAGEFRAME_ALLOC);
        if (new_pdpt == NULL) PANIC("Failed to allocate 4KiB page for PDPT");
        memset_pg(new_pdpt);

        pml4t_phys_addr[pml4t_idx] = (uint64_t)new_pdpt | PTE_PRESENT | PTE_WRITABLE;
    }
    uint64_t* pdpt = (uint64_t*)(pml4t_phys_addr[pml4t_idx] & PTE_ADDR_MASK);

    if (!(pdpt[pdpt_idx] & PTE_PRESENT)) {
        uint64_t* new_pd = (uint64_t*)phys_kmalloc(4096, PAGEFRAME_ALLOC);
        if (new_pd == NULL) PANIC("Failed to allocate 4KiB page for PD");
        memset_pg(new_pd);

        pdpt[pdpt_idx] = (uint64_t)new_pd | PTE_PRESENT | PTE_WRITABLE;
    }
    uint64_t* pd = (uint64_t*)(pdpt[pdpt_idx] & PTE_ADDR_MASK);

    if (!(pd[pd_idx] & PTE_PRESENT)) {
        uint64_t* new_pt = (uint64_t*)phys_kmalloc(4096, PAGEFRAME_ALLOC);
        if (new_pt == NULL) PANIC("Failed to allocate 4KiB page for PT");
        memset_pg(new_pt);

        pd[pd_idx] = (uint64_t)new_pt | PTE_PRESENT | PTE_WRITABLE;
    }
    uint64_t* pt = (uint64_t*)(pd[pd_idx] & PTE_ADDR_MASK);

    for (uint32_t idx = 0; idx < 4; idx++)
        pt[pt_idx + idx] = ((uint64_t)phys_stack_addr + idx * PAGE_SIZE) | PTE_FLAGS_KERNEL_DATA;
        // stack permissions are like kernel data section

    return (uint64_t*)(KERNEL_STACKS_START + 4 * PAGE_SIZE - 8);
    // last 8-byte aligned slot in mapped region; after ret pops it, RSP lands 16-byte aligned
}


static void map_direct_mapping(void) {
    // reads e820 map and maps usable RAM
    for (uint32_t i = 0; i < mmap_entry_count; i++) {
        if (mmap_bios_entries[i].type != 0x01) continue;

        void* base_addr = mmap_bios_entries[i].base_address;
        uint64_t size = mmap_bios_entries[i].chunk_size;
        const void* last_addr = (void*)((uint64_t)base_addr + size);

        // align UP physical address to 4KB
        uint64_t base_addr_aligned = ((uint64_t)base_addr + PAGE_SIZE - 1) & ~((uint64_t)PAGE_SIZE - 1);
        const void* vaddr = (void*)((uint64_t)DIRECT_MAP_START + base_addr_aligned);
        // should be a valid aligned virtual address

        const uint64_t pml4t_idx = VIRT_TO_PML4_IDX((uint64_t)vaddr);
        const uint64_t pdpt_idx = VIRT_TO_PDPT_IDX((uint64_t)vaddr);
        const uint64_t pd_idx = VIRT_TO_PD_IDX((uint64_t)vaddr);
        uint64_t pt_idx = VIRT_TO_PT_IDX((uint64_t)vaddr);

        // wire pml to pt
        if (!(pml4t_phys_addr[pml4t_idx] & PTE_PRESENT)) {
            uint64_t* new_pdpt = (uint64_t*)phys_kmalloc(4096, PAGEFRAME_ALLOC);
            if (new_pdpt == NULL) PANIC("Failed to allocate 4KiB page for PDPT");
            memset_pg(new_pdpt);
            pml4t_phys_addr[pml4t_idx] = (uint64_t)new_pdpt | PTE_PRESENT | PTE_WRITABLE;
        }
        uint64_t* pdpt = (uint64_t*)(pml4t_phys_addr[pml4t_idx] & PTE_ADDR_MASK);

        if (!(pdpt[pdpt_idx] & PTE_PRESENT)) {
            uint64_t* new_pd = (uint64_t*)phys_kmalloc(4096, PAGEFRAME_ALLOC);
            if (new_pd == NULL) PANIC("Failed to allocate 4KiB page for PD");
            memset_pg(new_pd);
            pdpt[pdpt_idx] = (uint64_t)new_pd | PTE_PRESENT | PTE_WRITABLE;
        }
        uint64_t* pd = (uint64_t*)(pdpt[pdpt_idx] & PTE_ADDR_MASK);


        // align DOWN last_addr
        const uint64_t last_addr_aligned = (uint64_t)last_addr & ~((uint64_t)PAGE_SIZE - 1);


        // map 4KB pages until first 2MB aligned address
        uint64_t huge_base = (base_addr_aligned + HUGE_PAGE_SIZE - 1) & ~(HUGE_PAGE_SIZE - 1); // align UP
        uint64_t huge_end  = last_addr_aligned & ~(HUGE_PAGE_SIZE - 1); // ALIGN DOWN

        if (huge_base >= huge_end) {
            // map with 4KB pages
            uint64_t* pt = (uint64_t*)phys_kmalloc(4096, PAGEFRAME_ALLOC);
            if (pt == NULL) PANIC("Failed to allocate 4KiB page for PT");
            memset_pg(pt);

            pd[pd_idx] = (uint64_t)pt | PTE_PRESENT | PTE_WRITABLE;

            uint64_t pte_num = ((uint64_t)last_addr_aligned - base_addr_aligned) / PAGE_SIZE;
            for (uint64_t k = 0; k < pte_num; k++) {
                pt[pt_idx + k] = ((uint64_t)base_addr_aligned + k * PAGE_SIZE) | PTE_FLAGS_DIRECT_MAP;
            }

        } else {
            uint64_t cur_pd_idx = pd_idx;

            if (base_addr_aligned < huge_base) {
                uint64_t* pre_pt = (uint64_t*)phys_kmalloc(4096, PAGEFRAME_ALLOC);
                if (pre_pt == NULL) PANIC("Failed to allocate 4KiB page for PT");
                memset_pg(pre_pt);
                pd[cur_pd_idx] = (uint64_t)pre_pt | PTE_PRESENT | PTE_WRITABLE;

                uint64_t pre_pte_num = (huge_base - base_addr_aligned) / PAGE_SIZE;
                for (uint64_t k = 0; k < pre_pte_num; k++) {
                    pre_pt[pt_idx + k] = ((uint64_t)base_addr_aligned + k * PAGE_SIZE) | PTE_FLAGS_DIRECT_MAP;
                }
                cur_pd_idx++;
            }

            uint64_t huge_count = (huge_end - huge_base) / HUGE_PAGE_SIZE;
            for (uint64_t j = 0; j < huge_count; j++) {
                pd[cur_pd_idx + j] = ((uint64_t)huge_base + j * HUGE_PAGE_SIZE) | PTE_FLAGS_DIRECT_MAP | PTE_HUGE_PAGE;
            }
            cur_pd_idx += huge_count;

            uint64_t leftover_pte_num = ((uint64_t)last_addr_aligned - huge_end) / PAGE_SIZE;
            if (leftover_pte_num > 0) {
                uint64_t* leftover_pt = (uint64_t*)phys_kmalloc(4096, PAGEFRAME_ALLOC);
                if (leftover_pt == NULL) PANIC("Failed to allocate 4KiB page for PT");
                memset_pg(leftover_pt);
                pd[cur_pd_idx] = (uint64_t)leftover_pt | PTE_PRESENT | PTE_WRITABLE;

                for (uint64_t k = 0; k < leftover_pte_num; k++)
                    leftover_pt[k] = ((uint64_t)huge_end + k * PAGE_SIZE) | PTE_FLAGS_DIRECT_MAP;
            }
        }
    }
}

void identity_map_rip(void) {
    // identity maps first 4MB
    const uint64_t pml4t_idx = 0;
    const uint64_t pdpt_idx = 0;
    const uint64_t pd_idx = 0;

    // wire pml4...
    if (!(pml4t_phys_addr[pml4t_idx] & PTE_PRESENT)) {
        uint64_t* new_pdpt = (uint64_t*)phys_kmalloc(4096, PAGEFRAME_ALLOC);
        if (new_pdpt == NULL) PANIC("Failed to allocate 4KiB page for PDPT");
        memset_pg(new_pdpt);

        pml4t_phys_addr[pml4t_idx] = (uint64_t)new_pdpt | PTE_PRESENT | PTE_WRITABLE;
    }
    uint64_t* pdpt = (uint64_t*)(pml4t_phys_addr[pml4t_idx] & PTE_ADDR_MASK);

    if (!(pdpt[pdpt_idx] & PTE_PRESENT)) {
        uint64_t* new_pd = (uint64_t*)phys_kmalloc(4096, PAGEFRAME_ALLOC);
        if (new_pd == NULL) PANIC("Failed to allocate 4KiB page for PD");
        memset_pg(new_pd);

        pdpt[pdpt_idx] = (uint64_t)new_pd | PTE_PRESENT | PTE_WRITABLE;
    }
    uint64_t* pd = (uint64_t*)(pdpt[pdpt_idx] & PTE_ADDR_MASK);

    pd[pd_idx] = 0ULL | PTE_PRESENT | PTE_WRITABLE | PTE_HUGE_PAGE;
    pd[pd_idx + 1] = (uint64_t) HUGE_PAGE_SIZE | PTE_PRESENT | PTE_WRITABLE | PTE_HUGE_PAGE;
}

static uint64_t* __init_mmu_page_tables_pml4(void) {
    // indices based on virtual address
    const uint64_t pml4t_idx = VIRT_TO_PML4_IDX(KERNEL_IMAGE_START);
    const uint64_t pdpt_idx = VIRT_TO_PDPT_IDX(KERNEL_IMAGE_START);
    const uint64_t pd_idx = VIRT_TO_PD_IDX(KERNEL_IMAGE_START);
    uint64_t pt_idx = VIRT_TO_PT_IDX(KERNEL_IMAGE_START);

    // allocate pml4, pdpt, pd, pt from buddy allocator and store in global variables
    pml4t_phys_addr = (uint64_t*)phys_kmalloc(4096, PAGEFRAME_ALLOC);
    if (pml4t_phys_addr == NULL) PANIC("Failed to allocate 4KiB page for PML4T");
    memset_pg(pml4t_phys_addr);

    // wire pml4 to pdpt to pd to pt
    if (!(pml4t_phys_addr[pml4t_idx] & PTE_PRESENT)) {
        uint64_t* new_pdpt = (uint64_t*)phys_kmalloc(4096, PAGEFRAME_ALLOC);
        if (new_pdpt == NULL) PANIC("Failed to allocate 4KiB page for PDPT");
        memset_pg(new_pdpt);

        pml4t_phys_addr[pml4t_idx] = (uint64_t)new_pdpt | PTE_PRESENT | PTE_WRITABLE;
    }
    uint64_t* pdpt = (uint64_t*)(pml4t_phys_addr[pml4t_idx] & PTE_ADDR_MASK);

    if (!(pdpt[pdpt_idx] & PTE_PRESENT)) {
        uint64_t* new_pd = (uint64_t*)phys_kmalloc(4096, PAGEFRAME_ALLOC);
        if (new_pd == NULL) PANIC("Failed to allocate 4KiB page for PD");
        memset_pg(new_pd);

        pdpt[pdpt_idx] = (uint64_t)new_pd | PTE_PRESENT | PTE_WRITABLE;
    }
    uint64_t* pd = (uint64_t*)(pdpt[pdpt_idx] & PTE_ADDR_MASK);

    if (!(pd[pd_idx] & PTE_PRESENT)) {
        uint64_t* new_pt = (uint64_t*)phys_kmalloc(4096, PAGEFRAME_ALLOC);
        if (new_pt == NULL) PANIC("Failed to allocate 4KiB page for PT");
        memset_pg(new_pt);

        pd[pd_idx] = (uint64_t)new_pt | PTE_PRESENT | PTE_WRITABLE;
    }
    uint64_t* pt = (uint64_t*)(pd[pd_idx] & PTE_ADDR_MASK);

    // map (_kernel_start to _text_end) to virtual address: KERNEL_IMAGE_START
    const uint64_t text_section_size = text_end_addr - kernel_start_addr;
    uint64_t pte_num = text_section_size / PAGE_SIZE; // text_end and kernel_start should be 4KB aligned

    uint32_t i = 0;
    while (i < pte_num) {
        pt[pt_idx + i] = ((uint64_t)kernel_start_addr + i * PAGE_SIZE) | PTE_FLAGS_KERNEL_CODE;
        i++;
    }
    pt_idx += i; // first pte not mapped after the first mapping
    // const uint64_t last_address_mapped = kernel_start_addr + i * PAGE_SIZE;

    // map (rodata start to rodata end) to KERNEL_BASED_PHYS_TO_VIRT(_rodata_start)
    const uint64_t rodata_section_size = rodata_end_addr - rodata_start_addr;
    pte_num = rodata_section_size / PAGE_SIZE;

    i = 0;
    while (i < pte_num) {
        pt[pt_idx + i] = ((uint64_t)rodata_start_addr + i * PAGE_SIZE) | PTE_PRESENT | PTE_NX | PTE_GLOBAL; // read-only
        i++;
    }
    pt_idx += i;

    // map (_data_start to _kernel_end) to KERNEL_BASED_PHYS_TO_VIRT(_data_start)
    const uint64_t data_section_size = kernel_end_addr - data_start_addr;
    pte_num = data_section_size / PAGE_SIZE;

    i = 0;
    while (i < pte_num) {
        pt[pt_idx + i] = ((uint64_t)data_start_addr + i * PAGE_SIZE) | PTE_FLAGS_KERNEL_DATA;
        i++;
    }

    // map stack
    virt_rsp = map_stacks_region();
    // direct map
    map_direct_mapping();
    // identity map for RIP validation during cr3 write
    identity_map_rip();
    // add VBE framebuffer to direct map
    map_vbe_fb();

    load_pmlt_cr3(pml4t_phys_addr); // mov cr3, pml4t

    const boot_vbe_handoff* vbe_info_local = (const boot_vbe_handoff*)(vbe_handoff_address);
    vesa_set_fb_virtual(DIRECT_MAP_START + vbe_info_local->framebuffer_address);

    return virt_rsp;
}


/* responsible for initialising VMA, rb-tree, page fault handler metadata and, rest of virtual memory/mmu subsystem...
 * pf fires->VMA lookup->classification of pf->flags determined->pf handler calls map_page functions->pf iretq's
 * WHAT ABOUT 1GB PAGES
*/


void map_page(const void* paddr, const void* vaddr, const uint64_t flags) {
    // what if flags have PTE_HUGE_PAGE?
    // align down to 4KB
    const uint64_t aligned_addr = (uint64_t)paddr & ~((uint64_t)PAGE_SIZE - 1);
    const uint64_t pte = aligned_addr | flags;
    
    const uint64_t pml4t_idx = VIRT_TO_PML4_IDX((uint64_t)vaddr);
    const uint64_t pdpt_idx = VIRT_TO_PDPT_IDX((uint64_t)vaddr);
    const uint64_t pd_idx = VIRT_TO_PD_IDX((uint64_t)vaddr);
    const uint64_t pt_idx = VIRT_TO_PT_IDX((uint64_t)vaddr);

    // wire pml4t...
    if (!(pml4t_phys_addr[pml4t_idx] & PTE_PRESENT)) {
        uint64_t* new_pdpt = (uint64_t*)phys_kmalloc(4096, PAGEFRAME_ALLOC);
        if (new_pdpt == NULL) PANIC("Failed to allocate 4KiB page for PDPT");
        memset_pg(new_pdpt);

        pml4t_phys_addr[pml4t_idx] = (uint64_t)new_pdpt | PTE_PRESENT | PTE_WRITABLE;
    }
    uint64_t* pdpt = (uint64_t*)(pml4t_phys_addr[pml4t_idx] & PTE_ADDR_MASK);

    if (!(pdpt[pdpt_idx] & PTE_PRESENT)) {
        uint64_t* new_pd = (uint64_t*)phys_kmalloc(4096, PAGEFRAME_ALLOC);
        if (new_pd == NULL) PANIC("Failed to allocate 4KiB page for PD");
        memset_pg(new_pd);

        pdpt[pdpt_idx] = (uint64_t)new_pd | PTE_PRESENT | PTE_WRITABLE;
    }
    uint64_t* pd = (uint64_t*)(pdpt[pdpt_idx] & PTE_ADDR_MASK);

    if (!(pd[pd_idx] & PTE_PRESENT)) {
        uint64_t* new_pt = (uint64_t*)phys_kmalloc(4096, PAGEFRAME_ALLOC);
        if (new_pt == NULL) PANIC("Failed to allocate 4KiB page for PT");
        memset_pg(new_pt);

        pd[pd_idx] = (uint64_t)new_pt | PTE_PRESENT | PTE_WRITABLE;
    }
    uint64_t* pt = (uint64_t*)(pd[pd_idx] & PTE_ADDR_MASK);

    pt[pt_idx] = pte;  
}

void map_huge_page(const void* paddr, const void* vaddr, const uint64_t flags) {
    // will cause a fault if flags don't have PTE_HUGE_PAGE
    const uint64_t pml4t_idx = VIRT_TO_PML4_IDX((uint64_t)vaddr);
    const uint64_t pdpt_idx = VIRT_TO_PDPT_IDX((uint64_t)vaddr);
    uint64_t pd_idx = VIRT_TO_PD_IDX((uint64_t)vaddr);

    //wire...
    if (!(pml4t_phys_addr[pml4t_idx] & PTE_PRESENT)) {
        uint64_t* new_pdpt = (uint64_t*)phys_kmalloc(4096, PAGEFRAME_ALLOC);
        if (new_pdpt == NULL) PANIC("Failed to allocate 4KiB page for PDPT");
        memset_pg(new_pdpt);

        pml4t_phys_addr[pml4t_idx] = (uint64_t)new_pdpt | PTE_PRESENT | PTE_WRITABLE;
    }    
    uint64_t* pdpt = (uint64_t*)(pml4t_phys_addr[pml4t_idx] & PTE_ADDR_MASK);

    if (!(pdpt[pdpt_idx] & PTE_PRESENT)) {
        uint64_t* new_pd = (uint64_t*)phys_kmalloc(4096, PAGEFRAME_ALLOC);
        if (new_pd == NULL) PANIC("Failed to allocate 4KiB page for PD");
        memset_pg(new_pd);

        pdpt[pdpt_idx] = (uint64_t)new_pd | PTE_PRESENT | PTE_WRITABLE;
    }
    uint64_t* pd = (uint64_t*)(pdpt[pdpt_idx] & PTE_ADDR_MASK);

    pd[pd_idx] = ((uint64_t)paddr) | flags;
}

void map_pages(const void* paddr, const void* vaddr, const uint64_t flags, const uint64_t num_pages) {
    if (num_pages == 0) return;

    if (flags & PTE_HUGE_PAGE) {
        uint64_t cur_vaddr = (uint64_t)vaddr;
        uint64_t cur_paddr = (uint64_t)paddr;

        uint64_t cur_pml4t_idx = ~0ULL;
        uint64_t cur_pdpt_idx  = ~0ULL;
        uint64_t* pdpt = NULL;
        uint64_t* pd   = NULL;

        for (uint64_t count = 0; count < num_pages; count++) {
            uint64_t new_pml4t_idx = VIRT_TO_PML4_IDX(cur_vaddr);
            uint64_t new_pdpt_idx  = VIRT_TO_PDPT_IDX(cur_vaddr);

            if (new_pml4t_idx != cur_pml4t_idx) {
                cur_pml4t_idx = new_pml4t_idx;
                if (!(pml4t_phys_addr[cur_pml4t_idx] & PTE_PRESENT)) {
                    uint64_t* new_pdpt = (uint64_t*)phys_kmalloc(4096, PAGEFRAME_ALLOC);
                    if (new_pdpt == NULL) PANIC("Failed to allocate 4KiB page for PDPT");
                    memset_pg(new_pdpt);
                    pml4t_phys_addr[cur_pml4t_idx] = (uint64_t)new_pdpt | PTE_PRESENT | PTE_WRITABLE;
                }
                pdpt = (uint64_t*)(pml4t_phys_addr[cur_pml4t_idx] & PTE_ADDR_MASK);
                cur_pdpt_idx = ~0ULL; // force pdpt-level re-check
            }

            if (new_pdpt_idx != cur_pdpt_idx) {
                cur_pdpt_idx = new_pdpt_idx;
                if (!(pdpt[cur_pdpt_idx] & PTE_PRESENT)) {
                    uint64_t* new_pd = (uint64_t*)phys_kmalloc(4096, PAGEFRAME_ALLOC);
                    if (new_pd == NULL) PANIC("Failed to allocate 4KiB page for PD");
                    memset_pg(new_pd);
                    pdpt[cur_pdpt_idx] = (uint64_t)new_pd | PTE_PRESENT | PTE_WRITABLE;
                }
                pd = (uint64_t*)(pdpt[cur_pdpt_idx] & PTE_ADDR_MASK);
            }

            pd[VIRT_TO_PD_IDX(cur_vaddr)] = cur_paddr | flags;
            cur_vaddr += HUGE_PAGE_SIZE;
            cur_paddr += HUGE_PAGE_SIZE;
        }
        return;
    }

    // 4KB pages
    uint64_t cur_vaddr = (uint64_t)vaddr;
    uint64_t cur_paddr = (uint64_t)paddr & ~((uint64_t)PAGE_SIZE - 1);

    uint64_t cur_pml4t_idx = ~0ULL;
    uint64_t cur_pdpt_idx  = ~0ULL;
    uint64_t cur_pd_idx    = ~0ULL;
    uint64_t* pdpt = NULL;
    uint64_t* pd   = NULL;
    uint64_t* pt   = NULL;

    for (uint64_t count = 0; count < num_pages; count++) {
        uint64_t new_pml4t_idx = VIRT_TO_PML4_IDX(cur_vaddr);
        uint64_t new_pdpt_idx  = VIRT_TO_PDPT_IDX(cur_vaddr);
        uint64_t new_pd_idx    = VIRT_TO_PD_IDX(cur_vaddr);

        if (new_pml4t_idx != cur_pml4t_idx) {
            cur_pml4t_idx = new_pml4t_idx;
            if (!(pml4t_phys_addr[cur_pml4t_idx] & PTE_PRESENT)) {
                uint64_t* new_pdpt = (uint64_t*)phys_kmalloc(4096, PAGEFRAME_ALLOC);
                if (new_pdpt == NULL) PANIC("Failed to allocate 4KiB page for PDPT");
                memset_pg(new_pdpt);
                
                pml4t_phys_addr[cur_pml4t_idx] = (uint64_t)new_pdpt | PTE_PRESENT | PTE_WRITABLE;
            }
            pdpt = (uint64_t*)(pml4t_phys_addr[cur_pml4t_idx] & PTE_ADDR_MASK);
            cur_pdpt_idx = ~0ULL;
        }

        if (new_pdpt_idx != cur_pdpt_idx) {
            cur_pdpt_idx = new_pdpt_idx;
            if (!(pdpt[cur_pdpt_idx] & PTE_PRESENT)) {
                uint64_t* new_pd = (uint64_t*)phys_kmalloc(4096, PAGEFRAME_ALLOC);
                if (new_pd == NULL) PANIC("Failed to allocate 4KiB page for PD");
                memset_pg(new_pd);

                pdpt[cur_pdpt_idx] = (uint64_t)new_pd | PTE_PRESENT | PTE_WRITABLE;
            }
            pd = (uint64_t*)(pdpt[cur_pdpt_idx] & PTE_ADDR_MASK);
            cur_pd_idx = ~0ULL;
        }

        if (new_pd_idx != cur_pd_idx) {
            cur_pd_idx = new_pd_idx;
            if (!(pd[cur_pd_idx] & PTE_PRESENT)) {
                uint64_t* new_pt = (uint64_t*)phys_kmalloc(4096, PAGEFRAME_ALLOC);
                if (new_pt == NULL) PANIC("Failed to allocate 4KiB page for PT");
                memset_pg(new_pt);

                pd[cur_pd_idx] = (uint64_t)new_pt | PTE_PRESENT | PTE_WRITABLE;
            }
            pt = (uint64_t*)(pd[cur_pd_idx] & PTE_ADDR_MASK);
            if (pt == NULL) PANIC("Failed to ptify mmu_page_tables.c:map_pages");
        }

        pt[VIRT_TO_PT_IDX(cur_vaddr)] = cur_paddr | flags;
        cur_vaddr += PAGE_SIZE;
        cur_paddr += PAGE_SIZE;
    }
}
