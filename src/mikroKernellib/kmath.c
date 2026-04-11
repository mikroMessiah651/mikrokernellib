#include "include/kmath.h"
#include "include/mikroKernellib-common.h"

uint32_t align_up(uint32_t x, uint32_t y) {
    return ((x + y - 1) / y) * y;
}

/* Returns the smallest order such that 2^order * PAGE_SIZE >= size.
 * e.g. size = 4096*3 → order 2, because 2^2 * 4096 = 16 KiB fits 3 pages.
*/
uint32_t klog2(size_t x) {
    uint32_t r = 0;
    while (x >>= 1)
        r++;
    return r;
}

uint32_t round_up_pow2(uint32_t x) {
    if (x == 0) return 1;
    x--;
    x |= x >> 1;
    x |= x >> 2;
    x |= x >> 4;
    x |= x >> 8;
    x |= x >> 16;
    return x + 1;
}

uint32_t buddy_order(uint64_t size) {
    uint64_t pages = size / PAGE_SIZE;
    uint32_t order = 0;
    while ((1ULL << order) < pages)
        order++;
    return order;
}