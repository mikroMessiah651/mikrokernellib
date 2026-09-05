#include "include/kstrings.h"
#include <stddef.h>

size_t kstring_length(const char* s) {
    if (!s)
        return 0;
    size_t len = 0;
    while (s[len])
        len++;
    return len;
}

void kstring_strcpy(const char* s1, void* dst, size_t dst_size) {
    if (!s1 || !dst || dst_size == 0)
        return;
    char* d = (char*)dst;
    size_t i = 0;
    while (i < dst_size - 1 && s1[i]) {
        d[i] = s1[i];
        i++;
    }
    d[i] = '\0';
}

int kstring_strcmp(const char* s1, const char* s2) {
    if (s1 == s2)
        return 0;
    if (!s1)
        return -1;
    if (!s2)
        return 1;
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return (unsigned char)*s1 - (unsigned char)*s2;
}

#include <stdint.h>
void* memset(void* ptr, int value, size_t num) {
    if (num == 0)
        return ptr;

    __asm__ __volatile__(
        /*
         * On modern x86-64 CPUs (Intel Ivy Bridge+ and AMD Zen+),
         * rep stosb triggers ERMS
         * The hardware automatically checks alignment, performs
         * cache-line zeroing, and sets memory faster than any
         * manual 64-bit or 128-bit unrolled C loop.
         */
        "rep stosb"
        : "+D"(ptr), "+c"(num)      // Outputs/Inputs: Destination (%rdi), Count (%rcx)
        : "a"((unsigned char)value) // Input: Value byte stored in %al
        : "memory"                  // Clobber list: Tells compiler memory has changed
    );

    return ptr;
}
