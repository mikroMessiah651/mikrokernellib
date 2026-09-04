/* Eyal Kaghanovich
 * kernel main entry
 * kernel page tables are set up
 * pmm is initialised, IDT is initialised in identity mapped low physical memory
 */

#include "include/idt.h"
#include "include/mapped_phys_kmalloc.h"
#include "include/mikroKernellib-common.h"
#include "include/mmu.h"
#include "include/paging.h"
#include "include/pmap.h"
#include "include/vesa_graphics_lib.h"
#include "include/vm.h"

static void vhello(void) {
    vbe_black_screen();
    vesa_nt_println("entered virtual kernel entry\0", 0, 0);
}

__attribute__((noreturn)) void kmain(void) {
    vhello();
    _reload_idt_virtual();
    _init_dm_pmm();

    __init_pmap();

    unmap_boot_memory();
    // unmap low 16MB

    PANIC("HELLO VIRTUAL\0");
}
