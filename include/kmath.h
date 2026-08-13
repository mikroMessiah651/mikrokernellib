#ifndef KMATH_H
#define KMATH_H
#include <stddef.h>
#include <stdint.h>

uint32_t klog2(size_t x);
uint32_t align_up(uint32_t x, uint32_t y);
uint32_t round_up_pow2(uint32_t x);
uint32_t buddy_order(uint64_t size);

#endif