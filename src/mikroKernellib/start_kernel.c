/* Eyal Kaghanovich
 * kernel main entry
 */

#include "include/idt.h"
#include "include/kmain.h"
#include "include/mikroKernellib-common.h"
#include "include/mmu.h"
#include "include/phys_kmalloc.h"
#include "include/vesa_graphics_lib.h"
#include <stdint.h>


static void khello(void) {
    vbe_black_screen();
    vesa_nt_println("Welcome to E.K's mikrokernellib\0", 0, 0);
}

__attribute__((section(".text.entry"))) void start_kernel(void) {
    // snapshot the bootloader's VBE handoff out of low memory before anything
    // can tear the identity map down under us
    __init_vbe_handoff();

    khello();
    // functions marked with __init are run only in single-threaded Bootstrap
    // processor mode and therefore do not require a lock
    __init_idt();

    __init_phys_kmalloc();
    uint64_t* rsp = __init_mmu();
    enter_virtual(rsp, (uint64_t)&kmain);

    PANIC("HALTING SYSTEM, KERNEL BOOTING RETURNED UNEXPECTEDLY!\0");
}
