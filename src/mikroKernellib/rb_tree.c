/* Eyal Kaghanovich
 * red-black tree VMA search
 *
 * Every rb_node pointer stored in a tree is whatever the allocator handed out.
 * For the kernel VMA tree that is a slab object, i.e. a PHYSICAL address, which
 * stops being directly dereferenceable once __init_mmu() swaps CR3. So every
 * dereference goes through directmap_p2v() while the stored links stay exactly
 * as they were. The translation is kind-aware, so a node that already lives at
 * a virtual address passes through untouched.
 */

#include "include/rb_tree.h"
#include "include/mmu.h"
#include "include/vesa_graphics_lib.h"
#include <stdbool.h>
#include <stdint.h>

// rb_delete(.., spinlock_t tree_lock);

void rb_print_tree(struct rb_node** root, int* row, const int col,
                   uint64_t (*addr_of_node)(struct rb_node*)) {
    struct rb_node* x = *root;
    if (x == NULL)
        return;

    // tree rotated 90 degrees clockwise: deeper nodes indent further right,
    // each node gets its own row. col tracks depth, row advances across the
    // whole tree.
    rb_print_tree(&directmap_p2v(x)->right, row, col + 4, addr_of_node);
    vesa_print_virt_addr(addr_of_node(x), *row, col);
    (*row)++;
    rb_print_tree(&directmap_p2v(x)->left, row, col + 4, addr_of_node);
}

static void rb_rotate_left(struct rb_node** root, struct rb_node* node) {
    struct rb_node* tmp = directmap_p2v(node)->right;
    directmap_p2v(node)->right = directmap_p2v(tmp)->left;
    if (directmap_p2v(tmp)->left != NULL)
        directmap_p2v(directmap_p2v(node)->right)->parent = node;

    directmap_p2v(tmp)->parent = directmap_p2v(node)->parent;
    if (directmap_p2v(node)->parent == NULL) {
        // x was root
        // set tmp as new root
        *root = tmp;
    } else if (node == directmap_p2v(directmap_p2v(node)->parent)->left) {
        // x was a left child
        directmap_p2v(directmap_p2v(node)->parent)->left = tmp;
    } else {
        // x was a right child
        directmap_p2v(directmap_p2v(node)->parent)->right = tmp;
    }

    directmap_p2v(tmp)->left = node;
    directmap_p2v(node)->parent = tmp;
}

static void rb_rotate_right(struct rb_node** root,
                            struct rb_node* node) {
    struct rb_node* tmp = directmap_p2v(node)->left;
    directmap_p2v(node)->left = directmap_p2v(tmp)->right;
    if (directmap_p2v(tmp)->right != NULL)
        directmap_p2v(directmap_p2v(node)->left)->parent = node;

    directmap_p2v(tmp)->parent = directmap_p2v(node)->parent;
    if (directmap_p2v(node)->parent == NULL) {
        // x was root
        // set tmp as new root
        *root = tmp;
    } else if (node == directmap_p2v(directmap_p2v(node)->parent)->right) {
        // x was a right child
        directmap_p2v(directmap_p2v(node)->parent)->right = tmp;
    } else {
        // x was a left child
        directmap_p2v(directmap_p2v(node)->parent)->left = tmp;
    }

    directmap_p2v(tmp)->right = node;
    directmap_p2v(node)->parent = tmp;
}

static struct rb_node* rb_find_exact(struct rb_node** root, struct rb_node* key,
                                     int (*cmp)(struct rb_node*,
                                                struct rb_node*)) {
    struct rb_node* current = *root;
    while (current != NULL) {
        const int result = cmp(key, current);
        if (result < 0) {
            current = directmap_p2v(current)->left;
        } else if (result > 0) {
            current = directmap_p2v(current)->right;
        } else {
            return current;
        }
    }
    return NULL;
}

static struct rb_node* rb_minimum(struct rb_node* node) {
    while (directmap_p2v(node)->left != NULL) {
        node = directmap_p2v(node)->left;
    }
    return node;
}

static inline void rb_transplant(struct rb_node** root, const struct rb_node* node,
                                 struct rb_node* replacement) {
    if (directmap_p2v(node)->parent == NULL) {
        *root = replacement;
    } else if (node == directmap_p2v(directmap_p2v(node)->parent)->left) {
        directmap_p2v(directmap_p2v(node)->parent)->left = replacement;
    } else {
        directmap_p2v(directmap_p2v(node)->parent)->right = replacement;
    }
    if (replacement != NULL) {
        directmap_p2v(replacement)->parent = directmap_p2v(node)->parent;
    }
}

static void rb_insert_fixup(struct rb_node** root, struct rb_node* x) {
    while (directmap_p2v(x)->parent != NULL && directmap_p2v(directmap_p2v(x)->parent)->color == RB_RED) {
        if (directmap_p2v(directmap_p2v(x)->parent)->parent == NULL)
            break;

        if (directmap_p2v(x)->parent == directmap_p2v(directmap_p2v(directmap_p2v(x)->parent)->parent)->left) {
            struct rb_node* uncle = directmap_p2v(directmap_p2v(directmap_p2v(x)->parent)->parent)->right;
            if (uncle != NULL && directmap_p2v(uncle)->color == RB_RED) {
                // case 1: uncle is red
                directmap_p2v(directmap_p2v(x)->parent)->color = RB_BLACK;
                directmap_p2v(uncle)->color = RB_BLACK;

                directmap_p2v(directmap_p2v(directmap_p2v(x)->parent)->parent)->color = RB_RED;
                x = directmap_p2v(directmap_p2v(x)->parent)->parent;
            } else {
                if (x == directmap_p2v(directmap_p2v(x)->parent)->right) {
                    // case 2: node is a right child
                    x = directmap_p2v(x)->parent;
                    rb_rotate_left(root, x);
                }
                // case 3: node is a left child
                directmap_p2v(directmap_p2v(x)->parent)->color = RB_BLACK;
                directmap_p2v(directmap_p2v(directmap_p2v(x)->parent)->parent)->color = RB_RED;
                rb_rotate_right(root, directmap_p2v(directmap_p2v(x)->parent)->parent);
            }
        } else {
            // directmap_p2v(node)->parent == directmap_p2v(directmap_p2v(directmap_p2v(node)->parent)->parent)->right
            // node's parent is a right child
            struct rb_node* uncle = directmap_p2v(directmap_p2v(directmap_p2v(x)->parent)->parent)->left;
            if (uncle != NULL && directmap_p2v(uncle)->color == RB_RED) {
                // uncle is red
                directmap_p2v(directmap_p2v(x)->parent)->color = RB_BLACK;
                directmap_p2v(uncle)->color = RB_BLACK;

                directmap_p2v(directmap_p2v(directmap_p2v(x)->parent)->parent)->color = RB_RED;
                x = directmap_p2v(directmap_p2v(x)->parent)->parent;
            } else {
                if (x == directmap_p2v(directmap_p2v(x)->parent)->left) {
                    // node is a left child (zig-zag mirror)
                    x = directmap_p2v(x)->parent;
                    rb_rotate_right(root, x);
                }
                // node is a right child
                directmap_p2v(directmap_p2v(x)->parent)->color = RB_BLACK;
                directmap_p2v(directmap_p2v(directmap_p2v(x)->parent)->parent)->color = RB_RED;
                rb_rotate_left(root, directmap_p2v(directmap_p2v(x)->parent)->parent);
            }
        }
    }
    struct rb_node* T = *root;
    directmap_p2v(T)->color = RB_BLACK;
}

struct rb_node* rb_insert(struct rb_node** root, struct rb_node* node,
                          int (*cmp)(struct rb_node*, struct rb_node*)) {
    directmap_p2v(node)->left = NULL;
    directmap_p2v(node)->right = NULL;

    // walk BST to find place for node
    struct rb_node* current = *root;
    struct rb_node* parent = NULL;

    while (current != NULL) {
        parent = current;
        const int result = cmp(node, current);
        if (result < 0) {
            // node.field < current.field
            current = directmap_p2v(current)->left;
        } else if (result > 0) {
            // node.field > current.field
            current = directmap_p2v(current)->right;
        } else {
            // duplicate
            return NULL;
        }
    }
    if (parent == NULL) {
        *root = node;
        directmap_p2v(node)->parent = NULL;
        directmap_p2v(node)->color = RB_BLACK;
        return node;
    }

    const int result = cmp(node, parent);
    if (result < 0) {
        directmap_p2v(parent)->left = node;
    } else if (result > 0) {
        directmap_p2v(parent)->right = node;
    } else {
        return NULL;
    }

    // set color and parent
    directmap_p2v(node)->parent = parent;
    directmap_p2v(node)->color = RB_RED;

    // fix rb-tree structure violations
    rb_insert_fixup(root, node);

#ifdef DEBUG
    assert(rb_validate(root));
#endif

    return node;
}

static void rb_delete_fixup(struct rb_node** root, struct rb_node* x,
                            struct rb_node* x_parent) {
    while (x != *root && (x == NULL || directmap_p2v(x)->color == RB_BLACK)) {
        if (x == directmap_p2v(x_parent)->left) {
            struct rb_node* sibling = directmap_p2v(x_parent)->right;

            // case 1: sibling is red
            if (sibling != NULL && directmap_p2v(sibling)->color == RB_RED) {
                directmap_p2v(sibling)->color = RB_BLACK;
                directmap_p2v(x_parent)->color = RB_RED;
                rb_rotate_left(root, x_parent);
                sibling = directmap_p2v(x_parent)->right;
            }

            // case 2: sibling is black, both sibling's children are black
            if ((directmap_p2v(sibling)->left == NULL || directmap_p2v(directmap_p2v(sibling)->left)->color == RB_BLACK) &&
                (directmap_p2v(sibling)->right == NULL || directmap_p2v(directmap_p2v(sibling)->right)->color == RB_BLACK)) {
                directmap_p2v(sibling)->color = RB_RED;
                x = x_parent;
                x_parent = directmap_p2v(x)->parent;
            } else {
                // case 3: sibling is black, sibling's right child is black
                if (directmap_p2v(sibling)->right == NULL ||
                    directmap_p2v(directmap_p2v(sibling)->right)->color == RB_BLACK) {
                    if (directmap_p2v(sibling)->left != NULL)
                        directmap_p2v(directmap_p2v(sibling)->left)->color = RB_BLACK;
                    directmap_p2v(sibling)->color = RB_RED;
                    rb_rotate_right(root, sibling);
                    sibling = directmap_p2v(x_parent)->right;
                }
                // case 4: sibling is black, sibling's right child is red
                directmap_p2v(sibling)->color = directmap_p2v(x_parent)->color;
                directmap_p2v(x_parent)->color = RB_BLACK;
                if (directmap_p2v(sibling)->right != NULL)
                    directmap_p2v(directmap_p2v(sibling)->right)->color = RB_BLACK;
                rb_rotate_left(root, x_parent);
                x = *root;
            }
        } else {
            struct rb_node* sibling = directmap_p2v(x_parent)->left;

            // case 1: sibling is red
            if (sibling != NULL && directmap_p2v(sibling)->color == RB_RED) {
                directmap_p2v(sibling)->color = RB_BLACK;
                directmap_p2v(x_parent)->color = RB_RED;
                rb_rotate_right(root, x_parent);
                sibling = directmap_p2v(x_parent)->left;
            }

            // case 2: sibling is black, both sibling's children are black
            if ((directmap_p2v(sibling)->right == NULL || directmap_p2v(directmap_p2v(sibling)->right)->color == RB_BLACK) &&
                (directmap_p2v(sibling)->left == NULL || directmap_p2v(directmap_p2v(sibling)->left)->color == RB_BLACK)) {
                directmap_p2v(sibling)->color = RB_RED;
                x = x_parent;
                x_parent = directmap_p2v(x)->parent;
            } else {
                // case 3: sibling is black, sibling's left child is black
                if (directmap_p2v(sibling)->left == NULL || directmap_p2v(directmap_p2v(sibling)->left)->color == RB_BLACK) {
                    if (directmap_p2v(sibling)->right != NULL)
                        directmap_p2v(directmap_p2v(sibling)->right)->color = RB_BLACK;
                    directmap_p2v(sibling)->color = RB_RED;
                    rb_rotate_left(root, sibling);
                    sibling = directmap_p2v(x_parent)->left;
                }
                // case 4: sibling is black, sibling's left child is red
                directmap_p2v(sibling)->color = directmap_p2v(x_parent)->color;
                directmap_p2v(x_parent)->color = RB_BLACK;
                if (directmap_p2v(sibling)->left != NULL)
                    directmap_p2v(directmap_p2v(sibling)->left)->color = RB_BLACK;
                rb_rotate_right(root, x_parent);
                x = *root;
            }
        }
    }
    if (x != NULL)
        directmap_p2v(x)->color = RB_BLACK;
}

bool rb_delete(struct rb_node** root, struct rb_node* key,
               int (*cmp)(struct rb_node*, struct rb_node*)) {
    struct rb_node* x = NULL;
    struct rb_node* x_parent = NULL;

    struct rb_node* node = rb_find_exact(root, key, cmp);
    if (node == NULL)
        return false;

    struct rb_node* y = node;
    uint8_t y_original_color = directmap_p2v(y)->color;

    // case 1, node has no left child
    if (directmap_p2v(node)->left == NULL) {
        x = directmap_p2v(node)->right;
        x_parent = directmap_p2v(node)->parent;
        rb_transplant(root, node, directmap_p2v(node)->right);
    } else if (directmap_p2v(node)->right == NULL) {
        // case 2, node has no right child
        x = directmap_p2v(node)->left;
        x_parent = directmap_p2v(node)->parent;
        rb_transplant(root, node, directmap_p2v(node)->left);
    } else {
        // node has two children
        // find inorder successor
        // leftmost node in right subtree
        y = rb_minimum(directmap_p2v(node)->right);
        y_original_color = directmap_p2v(y)->color;
        x = directmap_p2v(y)->right;

        if (directmap_p2v(y)->parent == node) {
            x_parent = y;
            if (x != NULL)
                directmap_p2v(x)->parent = y;
        } else {
            rb_transplant(root, y, directmap_p2v(y)->right);
            directmap_p2v(y)->right = directmap_p2v(node)->right;
            directmap_p2v(directmap_p2v(y)->right)->parent = y;
            x_parent = directmap_p2v(y)->parent;
        }

        rb_transplant(root, node, y);
        directmap_p2v(y)->left = directmap_p2v(node)->left;
        directmap_p2v(directmap_p2v(y)->left)->parent = y;
        directmap_p2v(y)->color = directmap_p2v(node)->color;
    }

    if (y_original_color == RB_BLACK) {
        rb_delete_fixup(root, x, x_parent);
    }

#ifdef DEBUG
    assert(rb_validate(root));
#endif

    return true;
}

void rb_traverse_inorder(struct rb_node* root,
                         void (*callback)(struct rb_node*)) {
    if (root == NULL)
        return;

    // left subtree
    rb_traverse_inorder(directmap_p2v(root)->left, callback);

    // current node
    callback(root);

    // right subtree
    rb_traverse_inorder(directmap_p2v(root)->right, callback);
}
