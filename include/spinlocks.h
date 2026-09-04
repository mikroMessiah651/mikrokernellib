
#pragma once
#include <stdint.h>

typedef struct {
    volatile uint16_t ticket;
    volatile uint16_t serving;
    uint32_t cpu_id;
    uint64_t rflags;
} __attribute__((aligned(64))) spinlock_t;
// aligned 64 is wasteful on many contended locks

void spinlock_acquire(spinlock_t* lk);
void spinlock_release(spinlock_t* lk);
void spinlock_init(spinlock_t* lk);