
#pragma once
#include <stdint.h>

typedef struct {
    volatile uint32_t lock;
    uint32_t cpu_id;
    uint64_t rflags;
} spinlock_t;

void spinlock_acquire(spinlock_t* lk);
void spinlock_release(spinlock_t* lk);
void spinlock_init(spinlock_t* lk);