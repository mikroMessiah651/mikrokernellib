#ifndef KSTRINGS_H
#define KSTRINGS_H

#include <stddef.h>

/* Returns the number of characters in s before the null terminator.
 * Returns 0 if s is NULL. */
size_t kstring_length(const char *s);

/* Copies the null-terminated string s1 into dst.
 * Does nothing if either pointer is NULL.
 * dst must have enough space to hold s1 including the null terminator. */
void kstring_strcpy(const char *s1, void *dst, size_t dst_size);

/* Compares two null-terminated strings lexicographically.
 * Returns  0 if equal, <0 if s1 < s2, >0 if s1 > s2.
 * NULL is treated as less than any non-NULL string. */
int kstring_strcmp(const char *s1, const char *s2);

#endif
