//
// Created by eyalka on 3/3/26.
//

#define hlt __asm__ volatile("hlt")
#define cli __asm__ volatile("cli")
#define sti __asm__ volatile("sti")
#define nop __asm__ volatile("nop")
#define outb(port, val) __asm__ volatile("outb %0, %1" ::"a"(val), "Nd"(port))
#define get_cr3(cr3) asm volatile("mov %%cr3, %0" : "=r"(cr3))