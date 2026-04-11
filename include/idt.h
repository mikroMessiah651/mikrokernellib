#pragma once

#include <stdint.h>


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

void __init_idt(void);
void idt_set_gate(uint8_t num, uint64_t base, uint16_t sel, uint8_t flags);
