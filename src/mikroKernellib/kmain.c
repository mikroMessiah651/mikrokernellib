/* Eyal Kaghanovich
 * kernel main entry
 * kernel page tables are set up
 * pmm is initialised, IDT is initialised in identity mapped low physical memory
 */

#include "include/idt.h"
#include "include/mikroKernellib-common.h"
#include "include/mmu.h"
#include "include/vesa_graphics_lib.h"
#include "include/vm.h"

static inline void vhello(void) {
    vbe_black_screen();
    vesa_nt_println("entered virtual kernel entry\0", 0, 0);
}

void kmain(void) {
    vhello();
    _reload_idt_virtual();

    PANIC("HELLO VIRTUAL\0");
}
