
#include "include/vga-graphics.h"
#include "include/kasm.h"
#include <stdint.h>

#define VGA_LAST_ROW 24

typedef struct {
    uint64_t r15, r14, r13, r12, r11, r10, r9, r8,rbp, rdi, rsi, rdx, rcx, rbx, rax;
    uint64_t vec, error_code;
    uint64_t rip, cs, rflags, rsp, ss;
} registers_t;

void isr_dispatch(registers_t* rdi) {
    switch (rdi->vec) {
        case 0:
            vga_nt_println("#DE  Divide Error, vector: 0x00", VGA_LAST_ROW, 0);
            while(1) { hlt; }
        case 1:
            vga_nt_println("#DB  Debug, vector: 0x01", VGA_LAST_ROW, 0);
            while(1) { hlt; }
        case 2:
            vga_nt_println("NMI  Non-Maskable Interrupt, vector: 0x02", VGA_LAST_ROW, 0);
            while(1) { hlt; }
        case 3:
            vga_nt_println("#BP  Breakpoint, vector: 0x03", VGA_LAST_ROW, 0);
            while(1) { hlt; }
        case 4:
            vga_nt_println("#OF  Overflow, vector: 0x04", VGA_LAST_ROW, 0);
            while(1) { hlt; }
        case 5:
            vga_nt_println("#BR  BOUND Range Exceeded, vector: 0x05", VGA_LAST_ROW, 0);
            while(1) { hlt; }
        case 6:
            vga_nt_println("#UD  Invalid Opcode, vector: 0x06", VGA_LAST_ROW, 0);
            while(1) { hlt; }
        case 7:
            vga_nt_println("#NM  Device Not Available, vector: 0x07", VGA_LAST_ROW, 0);
            while(1) { hlt; }
        case 8:
            vga_nt_println("#DF  Double Fault, vector: 0x08", VGA_LAST_ROW, 0);
            while(1) { hlt; }
        case 9:
            vga_nt_println("Coprocessor Segment Overrun, vector: 0x09", VGA_LAST_ROW, 0);
            while(1) { hlt; }
        case 10:
            vga_nt_println("#TS  Invalid TSS, vector: 0x0A", VGA_LAST_ROW, 0);
            while(1) { hlt; }
        case 11:
            vga_nt_println("#NP  Segment Not Present, vector: 0x0B", VGA_LAST_ROW, 0);
            while(1) { hlt; }
        case 12:
            vga_nt_println("#SS  Stack-Segment Fault, vector: 0x0C", VGA_LAST_ROW, 0);
            while(1) { hlt; }
        case 13:
            vga_nt_println("#GP  General Protection fault, vector: 0x0D", VGA_LAST_ROW, 0);
            while(1) { hlt; }
        case 14:
            vga_nt_println("#PF  Page Fault, vector: 0x0E", VGA_LAST_ROW, 0);
            while(1) { hlt; }
        case 15:
            vga_nt_println("Reserved, vector: 0x0F", VGA_LAST_ROW, 0);
            while(1) { hlt; }
        case 16:
            vga_nt_println("#MF  x87 FPU Error, vector: 0x10", VGA_LAST_ROW, 0);
            while(1) { hlt; }
        case 17:
            vga_nt_println("#AC  Alignment Check, vector: 0x11", VGA_LAST_ROW, 0);
            while(1) { hlt; }
        case 18:
            vga_nt_println("#MC  Machine Check, vector: 0x12", VGA_LAST_ROW, 0);
            while(1) { hlt; }
        case 19:
            vga_nt_println("#XM  SIMD Floating-Point Exception, vector: 0x13", VGA_LAST_ROW, 0);
            while(1) { hlt; }
        case 20:
            vga_nt_println("#VE  Virtualization Exception, vector: 0x14", VGA_LAST_ROW, 0);
            while(1) { hlt; }
        case 21:
            vga_nt_println("#CP  Control Protection, vector: 0x15", VGA_LAST_ROW, 0);
            while(1) { hlt; }
        case 22:
            vga_nt_println("Reserved, vector: 0x16", VGA_LAST_ROW, 0);
            while(1) { hlt; }
        case 23:
            vga_nt_println("Reserved, vector: 0x17", VGA_LAST_ROW, 0);
            while(1) { hlt; }
        case 24:
            vga_nt_println("Reserved, vector: 0x18", VGA_LAST_ROW, 0);
            while(1) { hlt; }
        case 25:
            vga_nt_println("Reserved, vector: 0x19", VGA_LAST_ROW, 0);
            while(1) { hlt; }
        case 26:
            vga_nt_println("Reserved, vector: 0x1A", VGA_LAST_ROW, 0);
            while(1) { hlt; }
        case 27:
            vga_nt_println("Reserved, vector: 0x1B", VGA_LAST_ROW, 0);
            while(1) { hlt; }
        case 28:
            vga_nt_println("#HV  Hypervisor Injection Exception, vector: 0x1C", VGA_LAST_ROW, 0);
            while(1) { hlt; }
        case 29:
            vga_nt_println("#VC  VMM Communication Exception, vector: 0x1D", VGA_LAST_ROW, 0);
            while(1) { hlt; }
        case 30:
            vga_nt_println("#SX  Security Exception, vector: 0x1E", VGA_LAST_ROW, 0);
            while(1) { hlt; }
        case 31:
            vga_nt_println("Reserved, vector: 0x1F", VGA_LAST_ROW, 0);
            while(1) { hlt; }
        default:
            vga_nt_println("Unhandled Interrupt", VGA_LAST_ROW, 0);
            while(1) { hlt; }
    }
}
