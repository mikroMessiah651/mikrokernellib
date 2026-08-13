/* Eyal Kaghanovich
 * kernel main entry
 */

#include "include/idt.h"
#include "include/kasm.h"
#include "include/kmain.h"
#include "include/mikroKernellib-common.h"
#include "include/mmu.h"
#include "include/phys_kmalloc.h"
#include "include/vesa_graphics_lib.h"
#include <include/kstrings.h>
#include <stdint.h>

void khello(void) {
    vbe_black_screen();
    vesa_nt_println("Welcome to E.K's mikrokernellib\0", 0, 0);
}

// __init_kernellib_phys->virtual->__init_kernellib_virt
__attribute__((section(".text.entry"))) void start_kernel(void) {
    khello();
    // functions marked with __init are run only in single-threaded Bootstrap
    // processor mode and therefore do not require a spinlock functions like
    // allocators do need spinlocks because they will be called by APs
    // concurrently
    __init_idt();
    // identity mapped lower-physical memory IDT

    __init_phys_kmalloc();
    uint64_t* rsp = __init_mmu();
    enter_virtual(rsp, PHYS_TO_VIRT(&kmain));

    PANIC("HALTING SYSTEM, KERNEL BOOTING RETURNED UNEXPECTEDLY!\0");
}
