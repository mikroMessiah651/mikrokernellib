#include "include/kstrings.h"

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