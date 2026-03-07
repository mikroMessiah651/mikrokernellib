#include "include/idt.h"

typedef struct {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed)) idtr_t;


typedef struct {
    uint16_t offset_low;
    uint16_t selector;
    uint8_t  zero;
    uint8_t  type_attr;
    uint16_t offset_mid;
    uint32_t offset_high;
    uint32_t zero2;
} __attribute__((packed)) IDT_gate_descriptor_64bit;


extern void isr0(void),  isr1(void),  isr2(void),  isr3(void),
            isr4(void),  isr5(void),  isr6(void),  isr7(void),
            isr8(void),  isr9(void),  isr10(void), isr11(void),
            isr12(void), isr13(void), isr14(void), isr15(void),
            isr16(void), isr17(void), isr18(void), isr19(void),
            isr20(void), isr21(void), isr22(void), isr23(void),
            isr24(void), isr25(void), isr26(void), isr27(void),
            isr28(void), isr29(void), isr30(void), isr31(void);

extern void idt_load(idtr_t* idtr);

static void (*const isr_stubs[32])(void) = {
    isr0,  isr1,  isr2,  isr3,  isr4,  isr5,  isr6,  isr7,
    isr8,  isr9,  isr10, isr11, isr12, isr13, isr14, isr15,
    isr16, isr17, isr18, isr19, isr20, isr21, isr22, isr23,
    isr24, isr25, isr26, isr27, isr28, isr29, isr30, isr31,
};


//IDT structure statically allocated in .bss section
static IDT_gate_descriptor_64bit idt[256] __attribute__((aligned(16)));

static idtr_t idtr;


void idt_init(void) {
    idtr.limit = sizeof(idt) - 1;
    idtr.base  = (uint64_t)idt;

    idt_set_gate(0, (uint64_t)isr_stubs[0], 0x08, 0x8e);
    idt_set_gate(1, (uint64_t)isr_stubs[1], 0x08, 0x8e);
    idt_set_gate(2, (uint64_t)isr_stubs[2], 0x08, 0x8e);
    idt_set_gate(3, (uint64_t)isr_stubs[3], 0x08, 0x8f);

    for (int i = 4; i < 32; i++) {
        idt_set_gate(i, (uint64_t)isr_stubs[i], 0x08, 0x8e);
    }
// entries 0-31 filled

    idt_load(&idtr);
}


// example: idt_set_gate(i, isr0, 0x08, 0x8e);
void idt_set_gate(uint8_t num, uint64_t base, uint16_t sel, uint8_t flags) {
    idt[num].offset_low  = base & 0xFFFF;
    idt[num].selector    = sel;
    idt[num].zero        = 0x00;
    idt[num].type_attr   = flags;
    idt[num].offset_mid  = (base >> 16) & 0xFFFF;
    idt[num].offset_high = (base >> 32) & 0xFFFFFFFF;
    idt[num].zero2       = 0x00000000;
}