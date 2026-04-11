/*
 *
*/


#include "include/vesa_graphics_lib.h"

#include "include/kasm.h"
#include "include/isr_dispatch.h"


extern void* mov_rax_cr2(void);

static void pf_handler(const registers_t* regs) {
    pf_info_t pf_info;
    pf_info.error_code = regs->error_code;
    pf_info.vaddr = mov_rax_cr2();

    // print debug info
    vesa_clear_lower_half();
    vesa_print_virt_addr((uint64_t)pf_info.vaddr, VESA_LAST_ROW - 2, 0);

    // print the error code
    vesa_print_virt_addr(regs->error_code, VESA_LAST_ROW - 1, 0);
}

void isr_dispatch(const registers_t* regs) {
    switch (regs->vec) {
        case 0:
            vesa_nt_println("#DE  Divide Error, vector: 0x00", VESA_LAST_ROW, 0);
            while(1) { hlt; }
            break;

        case 1:
            vesa_nt_println("#DB  Debug, vector: 0x01", VESA_LAST_ROW, 0);
            while(1) { hlt; }
            break;

        case 2:
            vesa_nt_println("NMI  Non-Maskable Interrupt, vector: 0x02", VESA_LAST_ROW, 0);
            while(1) { hlt; }
            break;

        case 3:
            vesa_nt_println("#BP  Breakpoint, vector: 0x03", VESA_LAST_ROW, 0);
            while(1) { hlt; }
            break;

        case 4:
            vesa_nt_println("#OF  Overflow, vector: 0x04", VESA_LAST_ROW, 0);
            while(1) { hlt; }
            break;

        case 5:
            vesa_nt_println("#BR  BOUND Range Exceeded, vector: 0x05", VESA_LAST_ROW, 0);
            while(1) { hlt; }
            break;

        case 6:
            vesa_nt_println("#UD  Invalid Opcode, vector: 0x06", VESA_LAST_ROW, 0);
            while(1) { hlt; }
            break;

        case 7:
            vesa_nt_println("#NM  Device Not Available, vector: 0x07", VESA_LAST_ROW, 0);
            while(1) { hlt; }
            break;

        case 8:
            vesa_nt_println("#DF  Double Fault, vector: 0x08", VESA_LAST_ROW, 0);
            while(1) { hlt; }
            break;

        case 9:
            vesa_nt_println("Coprocessor Segment Overrun, vector: 0x09", VESA_LAST_ROW, 0);
            while(1) { hlt; }
            break;

        case 10:
            vesa_nt_println("#TS  Invalid TSS, vector: 0x0A", VESA_LAST_ROW, 0);
            while(1) { hlt; }
            break;

        case 11:
            vesa_nt_println("#NP  Segment Not Present, vector: 0x0B", VESA_LAST_ROW, 0);
            while(1) { hlt; }
            break;

        case 12:
            vesa_nt_println("#SS  Stack-Segment Fault, vector: 0x0C", VESA_LAST_ROW, 0);
            while(1) { hlt; }
            break;

        case 13:
            vesa_nt_println("#GP  General Protection fault, vector: 0x0D", VESA_LAST_ROW, 0);
            while(1) { hlt; }
            break;

        case 14:
            pf_handler(regs);
            vesa_nt_println("#PF  Page Fault, vector: 0x0E", VESA_LAST_ROW, 0);
            while(1) { hlt; }
            break;

        case 15:
            vesa_nt_println("Reserved, vector: 0x0F", VESA_LAST_ROW, 0);
            while(1) { hlt; }
            break;

        case 16:
            vesa_nt_println("#MF  x87 FPU Error, vector: 0x10", VESA_LAST_ROW, 0);
            while(1) { hlt; }
            break;

        case 17:
            vesa_nt_println("#AC  Alignment Check, vector: 0x11", VESA_LAST_ROW, 0);
            while(1) { hlt; }
            break;

        case 18:
            vesa_nt_println("#MC  Machine Check, vector: 0x12", VESA_LAST_ROW, 0);
            while(1) { hlt; }
            break;

        case 19:
            vesa_nt_println("#XM  SIMD Floating-Point Exception, vector: 0x13", VESA_LAST_ROW, 0);
            while(1) { hlt; }
            break;

        case 20:
            vesa_nt_println("#VE  Virtualization Exception, vector: 0x14", VESA_LAST_ROW, 0);
            while(1) { hlt; }
            break;

        case 21:
            vesa_nt_println("#CP  Control Protection, vector: 0x15", VESA_LAST_ROW, 0);
            while(1) { hlt; }
            break;

        case 22:
            vesa_nt_println("Reserved, vector: 0x16", VESA_LAST_ROW, 0);
            while(1) { hlt; }
            break;

        case 23:
            vesa_nt_println("Reserved, vector: 0x17", VESA_LAST_ROW, 0);
            while(1) { hlt; }
            break;

        case 24:
            vesa_nt_println("Reserved, vector: 0x18", VESA_LAST_ROW, 0);
            while(1) { hlt; }
            break;

        case 25:
            vesa_nt_println("Reserved, vector: 0x19", VESA_LAST_ROW, 0);
            while(1) { hlt; }
            break;

        case 26:
            vesa_nt_println("Reserved, vector: 0x1A", VESA_LAST_ROW, 0);
            while(1) { hlt; }
            break;

        case 27:
            vesa_nt_println("Reserved, vector: 0x1B", VESA_LAST_ROW, 0);
            while(1) { hlt; }
            break;

        case 28:
            vesa_nt_println("#HV  Hypervisor Injection Exception, vector: 0x1C", VESA_LAST_ROW, 0);
            while(1) { hlt; }
            break;

        case 29:
            vesa_nt_println("#VC  VMM Communication Exception, vector: 0x1D", VESA_LAST_ROW, 0);
            while(1) { hlt; }
            break;

        case 30:
            vesa_nt_println("#SX  Security Exception, vector: 0x1E", VESA_LAST_ROW, 0);
            while(1) { hlt; }
            break;

        case 31:
            vesa_nt_println("Reserved, vector: 0x1F", VESA_LAST_ROW, 0);
            while(1) { hlt; }
            break;

        default:
            vesa_nt_println("Unhandled Interrupt", VESA_LAST_ROW, 0);
            while(1) { hlt; }
            break;

    }
}