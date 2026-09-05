#pragma once

#include "spinlocks.h"
#include <stdint.h>

struct mm {
    offset_t phys_pml4t;
    struct rb_node* vma_tree;
    spinlock_t* mm_lock;
};

typedef int pid_t;

enum state: uint8_t {
    TASK_RUNNING,
    TASK_STOPPED,
    TASK_TRACED,
    TASK_INTERRUPTIBLE,
    TASK_UNINTERRUPTIBLE,
    EXIT_ZOMBIE,
    EXIT_DEAD,
    TASK_DEAD,
};

typedef enum state state_t;

struct task_struct {
    pid_t pid;
    pid_t tgid;
    //state_t state;
    struct mm* task_mm;

    // task_struct* parent, *children, *sibling, *tg_owner

    // sched info, prio...

    // filesystem info

    // security stuff
};