/*
 * Author: Eyal Kaghanovich
 * kernel main entry
*/


#include <include/kstrings.h>
#include <stdint.h>
#include "include/mikroKernellib-common.h"
#include "include/idt.h"
#include "include/kasm.h"
#include "include/mmu_page_tables.h"
#include "include/phys_kmalloc.h"
#include "include/vesa_graphics_lib.h"


void khello(void);
void __init_virtual_memory(void);
void __init_virt_kmalloc(void);


__attribute__((section(".text.entry"))) 
void __init_kernellib(void) {
    khello();
    // functions marked with __init are run only in single-threaded Bootstrap processor mode
    // and therefore do not require a spinlock
    // functions like allocators do need spinlocks because they will be called by APs concurrently
    __init_idt();
    __init_phys_kmalloc();
    __init_virtual_memory();
    __init_virt_kmalloc();

    PANIC("HALTING SYSTEM, GOOD PANIC!\0");
}

void khello(void) {
    vbe_black_screen();
    vesa_nt_println("Welcome to E.K's mikroKernellib\0", 0, 0);
}

__attribute__((noreturn)) 
void __init_virtual_memory(void) {
    uint64_t* rsp = __init_mmu_paging();
    jump_to_virt_and_switch_stack(rsp, KERNEL_IMAGE_START - KERNEL_PHYS_BASE);
}

void __init_virt_kmalloc(void) {
    PANIC("not implemented yet\0");
}