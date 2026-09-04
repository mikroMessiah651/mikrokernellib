/* Eyal Kaghanovich
 * red-black tree VMA search
 */

#include "include/rb_tree.h"
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
    rb_print_tree(&x->right, row, col + 4, addr_of_node);
    vesa_print_virt_addr(addr_of_node(x), *row, col);
    (*row)++;
    rb_print_tree(&x->left, row, col + 4, addr_of_node);
}

static void rb_rotate_left(struct rb_node** root, struct rb_node* node) {
    struct rb_node* tmp = node->right;
    node->right = tmp->left;
    if (tmp->left != NULL)
        node->right->parent = node;

    tmp->parent = node->parent;
    if (node->parent == NULL) {
        // x was root
        // set tmp as new root
        *root = tmp;
    } else if (node == node->parent->left) {
        // x was a left child
        node->parent->left = tmp;
    } else {
        // x was a right child
        node->parent->right = tmp;
    }

    tmp->left = node;
    node->parent = tmp;
}

static void rb_rotate_right(struct rb_node** root,
                            struct rb_node* node) {
    struct rb_node* tmp = node->left;
    node->left = tmp->right;
    if (tmp->right != NULL)
        node->left->parent = node;

    tmp->parent = node->parent;
    if (node->parent == NULL) {
        // x was root
        // set tmp as new root
        *root = tmp;
    } else if (node == node->parent->right) {
        // x was a right child
        node->parent->right = tmp;
    } else {
        // x was a left child
        node->parent->left = tmp;
    }

    tmp->right = node;
    node->parent = tmp;
}

static struct rb_node* rb_find_exact(struct rb_node** root, struct rb_node* key,
                                     int (*cmp)(struct rb_node*,
                                                struct rb_node*)) {
    struct rb_node* current = *root;
    while (current != NULL) {
        const int result = cmp(key, current);
        if (result < 0) {
            current = current->left;
        } else if (result > 0) {
            current = current->right;
        } else {
            return current;
        }
    }
    return NULL;
}

static struct rb_node* rb_minimum(struct rb_node* node) {
    while (node->left != NULL) {
        node = node->left;
    }
    return node;
}

static inline void rb_transplant(struct rb_node** root, const struct rb_node* node,
                                 struct rb_node* replacement) {
    if (node->parent == NULL) {
        *root = replacement;
    } else if (node == node->parent->left) {
        node->parent->left = replacement;
    } else {
        node->parent->right = replacement;
    }
    if (replacement != NULL) {
        replacement->parent = node->parent;
    }
}

static void rb_insert_fixup(struct rb_node** root, struct rb_node* x) {
    while (x->parent != NULL && x->parent->color == RB_RED) {
        if (x->parent->parent == NULL)
            break;

        if (x->parent == x->parent->parent->left) {
            struct rb_node* uncle = x->parent->parent->right;
            if (uncle != NULL && uncle->color == RB_RED) {
                // case 1: uncle is red
                x->parent->color = RB_BLACK;
                uncle->color = RB_BLACK;

                x->parent->parent->color = RB_RED;
                x = x->parent->parent;
            } else {
                if (x == x->parent->right) {
                    // case 2: node is a right child
                    x = x->parent;
                    rb_rotate_left(root, x);
                }
                // case 3: node is a left child
                x->parent->color = RB_BLACK;
                x->parent->parent->color = RB_RED;
                rb_rotate_right(root, x->parent->parent);
            }
        } else {
            // node->parent == node->parent->parent->right
            // node's parent is a right child
            struct rb_node* uncle = x->parent->parent->left;
            if (uncle != NULL && uncle->color == RB_RED) {
                // uncle is red
                x->parent->color = RB_BLACK;
                uncle->color = RB_BLACK;

                x->parent->parent->color = RB_RED;
                x = x->parent->parent;
            } else {
                if (x == x->parent->left) {
                    // node is a left child (zig-zag mirror)
                    x = x->parent;
                    rb_rotate_right(root, x);
                }
                // node is a right child
                x->parent->color = RB_BLACK;
                x->parent->parent->color = RB_RED;
                rb_rotate_left(root, x->parent->parent);
            }
        }
    }
    struct rb_node* T = *root;
    T->color = RB_BLACK;
}

struct rb_node* rb_insert(struct rb_node** root, struct rb_node* node,
                          int (*cmp)(struct rb_node*, struct rb_node*)) {
    node->left = NULL;
    node->right = NULL;

    // walk BST to find place for node
    struct rb_node* current = *root;
    struct rb_node* parent = NULL;

    while (current != NULL) {
        parent = current;
        const int result = cmp(node, current);
        if (result < 0) {
            // node.field < current.field
            current = current->left;
        } else if (result > 0) {
            // node.field > current.field
            current = current->right;
        } else {
            // duplicate
            return NULL;
        }
    }
    if (parent == NULL) {
        *root = node;
        node->parent = NULL;
        node->color = RB_BLACK;
        return node;
    }

    const int result = cmp(node, parent);
    if (result < 0) {
        parent->left = node;
    } else if (result > 0) {
        parent->right = node;
    } else {
        return NULL;
    }

    // set color and parent
    node->parent = parent;
    node->color = RB_RED;

    // fix rb-tree structure violations
    rb_insert_fixup(root, node);

#ifdef DEBUG
    assert(rb_validate(root));
#endif

    return node;
}

static void rb_delete_fixup(struct rb_node** root, struct rb_node* x,
                            struct rb_node* x_parent) {
    while (x != *root && (x == NULL || x->color == RB_BLACK)) {
        if (x == x_parent->left) {
            struct rb_node* sibling = x_parent->right;

            // case 1: sibling is red
            if (sibling != NULL && sibling->color == RB_RED) {
                sibling->color = RB_BLACK;
                x_parent->color = RB_RED;
                rb_rotate_left(root, x_parent);
                sibling = x_parent->right;
            }

            // case 2: sibling is black, both sibling's children are black
            if ((sibling->left == NULL || sibling->left->color == RB_BLACK) &&
                (sibling->right == NULL || sibling->right->color == RB_BLACK)) {
                sibling->color = RB_RED;
                x = x_parent;
                x_parent = x->parent;
            } else {
                // case 3: sibling is black, sibling's right child is black
                if (sibling->right == NULL ||
                    sibling->right->color == RB_BLACK) {
                    if (sibling->left != NULL)
                        sibling->left->color = RB_BLACK;
                    sibling->color = RB_RED;
                    rb_rotate_right(root, sibling);
                    sibling = x_parent->right;
                }
                // case 4: sibling is black, sibling's right child is red
                sibling->color = x_parent->color;
                x_parent->color = RB_BLACK;
                if (sibling->right != NULL)
                    sibling->right->color = RB_BLACK;
                rb_rotate_left(root, x_parent);
                x = *root;
            }
        } else {
            struct rb_node* sibling = x_parent->left;

            // case 1: sibling is red
            if (sibling != NULL && sibling->color == RB_RED) {
                sibling->color = RB_BLACK;
                x_parent->color = RB_RED;
                rb_rotate_right(root, x_parent);
                sibling = x_parent->left;
            }

            // case 2: sibling is black, both sibling's children are black
            if ((sibling->right == NULL || sibling->right->color == RB_BLACK) &&
                (sibling->left == NULL || sibling->left->color == RB_BLACK)) {
                sibling->color = RB_RED;
                x = x_parent;
                x_parent = x->parent;
            } else {
                // case 3: sibling is black, sibling's left child is black
                if (sibling->left == NULL || sibling->left->color == RB_BLACK) {
                    if (sibling->right != NULL)
                        sibling->right->color = RB_BLACK;
                    sibling->color = RB_RED;
                    rb_rotate_left(root, sibling);
                    sibling = x_parent->left;
                }
                // case 4: sibling is black, sibling's left child is red
                sibling->color = x_parent->color;
                x_parent->color = RB_BLACK;
                if (sibling->left != NULL)
                    sibling->left->color = RB_BLACK;
                rb_rotate_right(root, x_parent);
                x = *root;
            }
        }
    }
    if (x != NULL)
        x->color = RB_BLACK;
}

bool rb_delete(struct rb_node** root, struct rb_node* key,
               int (*cmp)(struct rb_node*, struct rb_node*)) {
    struct rb_node* x = NULL;
    struct rb_node* x_parent = NULL;

    struct rb_node* node = rb_find_exact(root, key, cmp);
    if (node == NULL)
        return false;

    struct rb_node* y = node;
    uint8_t y_original_color = y->color;

    // case 1, node has no left child
    if (node->left == NULL) {
        x = node->right;
        x_parent = node->parent;
        rb_transplant(root, node, node->right);
    } else if (node->right == NULL) {
        // case 2, node has no right child
        x = node->left;
        x_parent = node->parent;
        rb_transplant(root, node, node->left);
    } else {
        // node has two children
        // find inorder successor
        // leftmost node in right subtree
        y = rb_minimum(node->right);
        y_original_color = y->color;
        x = y->right;

        if (y->parent == node) {
            x_parent = y;
            if (x != NULL)
                x->parent = y;
        } else {
            rb_transplant(root, y, y->right);
            y->right = node->right;
            y->right->parent = y;
            x_parent = y->parent;
        }

        rb_transplant(root, node, y);
        y->left = node->left;
        y->left->parent = y;
        y->color = node->color;
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
    rb_traverse_inorder(root->left, callback);

    // current node
    callback(root);

    // right subtree
    rb_traverse_inorder(root->right, callback);
}
