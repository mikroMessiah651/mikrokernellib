#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define RB_RED 0
#define RB_BLACK 1

struct rb_node {
    struct rb_node* right;
    struct rb_node* left;
    struct rb_node* parent;
    uint8_t color;
};

bool rb_delete(struct rb_node** root, struct rb_node* key,
               int (*cmp)(struct rb_node*, struct rb_node*));
// removes the node entry with vma_start

struct rb_node* rb_insert(struct rb_node** root, struct rb_node* node,
                          int (*cmp)(struct rb_node*, struct rb_node*));
// insert node

struct rb_node* rb_find_exact_node(struct rb_node** root, uint64_t vma_start,
                                   int (*cmp)(struct rb_node*,
                                              struct rb_node*));
// returns node with vma_start

void rb_traverse_inorder(struct rb_node* root,
                         void (*callback)(struct rb_node*));
// calls the function passed to it on every node, in order

void rb_print_tree(struct rb_node** root, int* row, int col,
                   uint64_t (*addr_of_node)(struct rb_node*));

#ifdef DEBUG

static int rb_validate_helper(struct rb_node* node, int black_count,
                              int* path_black_count) {
    if (node == NULL) {
        if (*path_black_count == -1) {
            *path_black_count = black_count;
        } else if (black_count != *path_black_count) {
            // violation: inconsistent black height
            return 0;
        }
        return 1;
    }

    // violation: red node has red child
    if (node->color == RB_RED) {
        if (node->left != NULL && node->left->color == RB_RED)
            return 0;
        if (node->right != NULL && node->right->color == RB_RED)
            return 0;
    }

    // violation: parent pointer inconsistency
    if (node->left != NULL && node->left->parent != node)
        return 0;
    if (node->right != NULL && node->right->parent != node)
        return 0;

    // violation: bst ordering
    if (node->left != NULL) {
        struct vma* v = container_of(node, struct vma, node);
        struct vma* vl = container_of(node->left, struct vma, node);
        if (vl->vma_start >= v->vma_start)
            return 0;
    }
    if (node->right != NULL) {
        struct vma* v = container_of(node, struct vma, node);
        struct vma* vr = container_of(node->right, struct vma, node);
        if (vr->vma_start <= v->vma_start)
            return 0;
    }

    if (node->color == RB_BLACK)
        black_count++;

    return rb_validate_helper(node->left, black_count, path_black_count) &&
           rb_validate_helper(node->right, black_count, path_black_count);
}

int rb_validate(struct rb_node** root) {
    if (*root == NULL)
        return 1;

    // violation: root must be black
    if ((*root)->color != RB_BLACK)
        return 0;

    // violation: root must have no parent
    if ((*root)->parent != NULL)
        return 0;

    int path_black_count = -1;
    return rb_validate_helper(*root, 0, &path_black_count);
}

#endif