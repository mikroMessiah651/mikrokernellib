#pragma once


#include <stdint.h>
#include <stddef.h>

struct rb_node {
    uint64_t* vma_start;
    uint64_t* vma_end;
    uint64_t vma_pte;

    uint8_t color;
    struct rb_node* right;
    struct rb_node* left;
    struct rb_node* parent;
};

#define RB_RED 0
#define RB_BLACK 1
