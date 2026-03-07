/*
    Author: Eyal Kaghanovich
*/


#include "include/mikroKernellib-common.h"
#include "include/idt.h"
#include "include/vga-graphics.h"
#include "include/kasm.h"
#include "include/phys_kmalloc.h"


__attribute__((section(".text.entry"))) void Init_Kernellib(void) {
    vga_clear_screen();
    vga_nt_println("Welcome to E.K's mikroKernellib\0", 0, 0);

    idt_init();
    buddy_init();

    void* ptr = buddy_alloc(1024);
    vga_nt_println("Allocated memory\0", 3, 0);
    if (ptr == NULL) {
        vga_nt_println("Failed to allocate memory\0", 3, 0);
    }
    buddy_free(ptr, 1024);

    while(TRUE) {
        // Halt and wait for interrupts
        hlt;
    }
    vga_nt_println("how did we get here?\0", 1, 0);
}