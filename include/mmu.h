#pragma once

#include <stdint.h>
/*
 * Kernel virtual address space layout (PML4 / PML5)
 *
 * PML4 kernel half base:  0xFFFF800000000000
 * PML5 kernel half base:  0xFF00000000000000
 *
 * All region bases are identical in both modes.
 * Only the page table walker depth differs at runtime.
 *
 * +----------------------------------+ 0xFFFF800000000000  (PML4 kernel half)
 * | 2MB guard hole                   |
 * +----------------------------------+ 0xFFFF800000200000  DIRECT_MAP_START
 * | Direct physical map       (64TB) |
 * +----------------------------------+ 0xFFFFBC0000200000  DIRECT_MAP_END
 * | 2MB guard hole                   |
 * +----------------------------------+ 0xFFFFBC0000400000  VIRT_KMALLOC_START
 * | vmalloc region             (4TB) |
 * +----------------------------------+ 0xFFFFFC0000400000  VIRT_KMALLOC_END
 * | 2MB guard hole                   |
 * +----------------------------------+ 0xFFFFFC0000600000  KERNEL_STACKS_START
 * | Kernel stacks              (2TB) |
 * +----------------------------------+ 0xFFFFFE0000600000  KERNEL_STACKS_END
 * | unmapped                         |
 * +----------------------------------+ 0xFFFFFEFFFFE00000  MODULE_REGION_START
 * | Kernel modules             (2GB) |
 * +----------------------------------+ 0xFFFFFF7FFFE00000  MODULE_REGION_END
 * | 2MB guard hole                   |
 * +----------------------------------+
 * | unmapped                         |
 * +----------------------------------+ 0xFFFFFFFF80000000  KERNEL_IMAGE_START
 * | Kernel image                     |
 * +----------------------------------+ 0xFFFFFFFFFFFFFFFF
 * AVATAR_THE_LAST_VIRTUAL_ADDRESS
 */

#define AVATAR_THE_LAST_VIRTUAL_ADDRESS 0xFFFFFFFFFFFFFFFFULL

/* PML4 / PML5 kernel half bases */
#define PML4_KERNEL_HALF_BASE 0xFFFF800000000000ULL
#define PML5_KERNEL_HALF_BASE 0xFF00000000000000ULL

/* Guard hole size */
#define GUARD_HOLE_SIZE 0x0000000000200000ULL /* 2MB */

/* Direct physical map */
#define DIRECT_MAP_START 0xFFFF800000200000ULL
#define DIRECT_MAP_END 0xFFFFBC0000200000ULL
#define DIRECT_MAP_SIZE 0x0000400000000000ULL /* 64TB */

/* Virtual kernel allocator (virt_kmalloc) */
#define VIRT_KMALLOC_START 0xFFFFBC0000400000ULL
#define VIRT_KMALLOC_END 0xFFFFFC0000400000ULL
#define VIRT_KMALLOC_SIZE 0x0000040000000000ULL /* 4TB */

/* Kernel stacks */
#define KERNEL_STACKS_START 0xFFFFFC0000600000ULL
#define KERNEL_STACKS_END 0xFFFFFE0000600000ULL
#define KERNEL_STACKS_SIZE 0x0000020000000000ULL /* 2TB */
#define KERNEL_STACK_SLOT_SIZE                                                                        \
    0x0000000000008000ULL                             /* 32KB per slot (16KB stack + guard rounded to \
                                                         power of 2) */
#define KERNEL_STACK_SIZE 0x0000000000004000ULL       /* 16KB usable stack */
#define KERNEL_STACK_GUARD_SIZE 0x0000000000001000ULL /* 4KB guard page */

/* Kernel module region */
#define MODULE_REGION_START 0xFFFFFEFFFFE00000ULL
#define MODULE_REGION_END 0xFFFFFF7FFFE00000ULL
#define MODULE_REGION_SIZE 0x0000000080000000ULL /* 2GB */

/* Kernel image */
#define KERNEL_IMAGE_START 0xFFFFFFFF80000000ULL
#define KERNEL_PHYS_BASE 0x13000ULL
#define KERNEL_BASED_PHYS_TO_VIRT(p) \
    ((uint64_t)(p) - KERNEL_PHYS_BASE + KERNEL_IMAGE_START)
// returns the virtual address of a physical address in the kernel code

/* PTE macros */
#define PTE_PRESENT 0x0000000000000001ULL
#define PTE_WRITABLE 0x0000000000000002ULL
#define PTE_USER 0x0000000000000004ULL
#define PTE_PWT 0x0000000000000008ULL
#define PTE_CACHE_DISABLE 0x0000000000000010ULL
#define PTE_ACCESSED \
    0x0000000000000020ULL // not very important since I don't care about setting
                          // pte accessed bit
#define PTE_DIRTY 0x0000000000000040ULL
#define PTE_HUGE_PAGE 0x0000000000000080ULL
#define PTE_GLOBAL 0x0000000000000100ULL
#define PTE_NX 0x8000000000000000ULL
#define PTE_GUARD_PAGE 0x4000000000000000ULL

#define PTE_ADDR_MASK 0x000FFFFFFFFFF000ULL

#define GET_PA_FROM_PTE(pte) (pte & PTE_ADDR_MASK)
#define GET_FLAGS_FROM_PTE(pte) (pte & ~PTE_ADDR_MASK)

/* virtual address to indices based on level */
#define VIRT_TO_PML5_IDX(virt) (((virt) >> 48) & 0x1FF)
#define VIRT_TO_PML4_IDX(virt) (((virt) >> 39) & 0x1FF)
#define VIRT_TO_PDPT_IDX(virt) (((virt) >> 30) & 0x1FF)
#define VIRT_TO_PD_IDX(virt) (((virt) >> 21) & 0x1FF)
#define VIRT_TO_PT_IDX(virt) (((virt) >> 12) & 0x1FF)

/* PTE setups for defined VMAs */
#define PTE_FLAGS_DIRECT_MAP \
    ((0ULL) | PTE_PRESENT | PTE_WRITABLE | PTE_GLOBAL | PTE_NX)

#define PTE_FLAGS_KERNEL_MODULES ((0ULL) | PTE_PRESENT | PTE_GLOBAL)

#define PTE_FLAGS_GUARD ((0ULL) | PTE_GUARD_PAGE)

#define PTE_FLAGS_KERNEL_CODE ((0ULL) | PTE_PRESENT | PTE_GLOBAL)

#define PTE_FLAGS_KERNEL_DATA \
    ((0ULL) | PTE_PRESENT | PTE_WRITABLE | PTE_NX | PTE_GLOBAL)

#define PTE_FLAGS_USER_DATA \
    (0ULL | PTE_PRESENT | PTE_WRITABLE | PTE_USER | PTE_NX)

#define PTE_FLAGS_USER_CODE (0ULL | PTE_PRESENT | PTE_USER)

#define PHYS_KERNEL_START 0x13000 /* where bootloader loaded the kernel */

#define KERNEL_VIRT_OFFSET (KERNEL_IMAGE_START - KERNEL_PHYS_BASE)
#define PHYS_TO_VIRT(p) ((uint64_t)(p) + KERNEL_VIRT_OFFSET)

/* Direct-map translation for allocator-returned addresses.
 *
 * Every pointer the buddy/slab allocators hand out, and every table address
 * stored inside a PTE, is a PHYSICAL address. Those are only dereferenceable
 * directly while an identity map is authoritative:
 *   - before load_pmlt_cr3(): the bootloader's 4GB identity map
 *   - after  load_pmlt_cr3(): nothing but the low 16MB of map_boot_memory(),
 *                             which unmap_boot_memory() then takes away
 * Past that point they have to be reached through the direct map.
 *
 * phys_map_offset is 0 while the identity map is authoritative and becomes
 * DIRECT_MAP_START immediately after our own CR3 is loaded. Stored values stay
 * physical at all times and only the dereference is translated, so nothing has
 * to be rewritten at the switch and structures built before it stay valid.
 *
 * Lives in .data, NOT .bss: .bss is NOBITS, is not part of kernel.bin and is
 * never zeroed, so a .bss copy would start as whatever the BIOS left behind.
 *
 */
extern uint64_t phys_map_offset;

/* A physical address is always below 2^48; every kernel virtual address is at
 * or above the PML4 kernel half base. So the compare tells the two kinds apart,
 * which keeps the translation idempotent and makes it safe on the lists that
 * legitimately mix both - a slab list threads physical slab descriptors onto a
 * sentinel that lives in the kernel image.
 *
 * Written as a statement expression so ptr is evaluated exactly once, which
 * matters because call sites nest these (directmap_p2v(directmap_p2v(x)->parent)).
 */
#define directmap_p2v(ptr)                                              \
    __extension__({                                                     \
        const uint64_t __dm_p = (uint64_t)(ptr);                        \
        (__typeof__(ptr))((__dm_p < PML4_KERNEL_HALF_BASE)              \
                              ? __dm_p + phys_map_offset                \
                              : __dm_p);                                \
    })
// physical address -> currently dereferenceable pointer, type preserved.
// Already-virtual pointers pass through untouched.

#define directmap_p2v_deref(ptr) (*directmap_p2v(ptr))
// deref through the direct map: directmap_p2v_deref(pt + idx) is pt[idx],
// directmap_p2v_deref(node).left is node->left

uint64_t pml5_detect(); // returns 0 (in rax) if pml5 isn't available, 1 if it
                        // is available

void invlpg(const void* addr);

uint64_t* get_current_core_cr3();
void load_pmlt_cr3(uint64_t* cr3);

__attribute__((noreturn)) void enter_virtual(void* virt_rsp,
                                             uint64_t virt_entry);

uint64_t* __init_mmu();

void map_page(uint64_t* pml4t, const void* paddr, const void* vaddr,
              const uint64_t flags);
void unmap_page(const uint64_t* pml4t, const void* paddr, const void* vaddr);

void map_huge_page(uint64_t* pml4t, const void* paddr, const void* vaddr,
                   const uint64_t flags);
void unmap_huge_page(uint64_t* pml4t, const void* paddr, const void* vaddr);

void map_pages(uint64_t* pml4t, const void* paddr, const void* vaddr,
               const uint64_t flags, const uint64_t num_pages);
void unmap_pages(uint64_t* pml4t, const void* paddr, const void* vaddr,
                 const uint64_t flags, const uint64_t num_pages);
