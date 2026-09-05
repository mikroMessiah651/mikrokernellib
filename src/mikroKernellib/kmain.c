/* Eyal Kaghanovich
 * kernel main entry
 * kernel page tables are set up
 * pmm is initialised, IDT is initialised in identity mapped low physical memory
 */

#include "include/idt.h"
#include "include/mapped_phys_kmalloc.h"
#include "include/mikroKernellib-common.h"
#include "include/paging.h"
#include "include/pmap.h"
#include "include/vesa_graphics_lib.h"

static void vhello(void) {
    vbe_black_screen();
    vesa_nt_println("entered virtual kernel entry\0", 0, 0);
}

// idk why does CLion yell at me to put static, kmain is referenced in start_kernel.c
// ReSharper disable once CppUseInternalLinkage
__attribute__((noreturn)) void kmain(void) {
    vhello();
    _reload_idt_virtual();

    // phys_map_offset was already flipped by __init_mmu(), right after it
    // loaded our CR3 - there is no second init step to run here.
    __init_pmap();

    unmap_boot_memory();
    // unmap low 16MB

    PANIC("HELLO VIRTUAL\0");
}
