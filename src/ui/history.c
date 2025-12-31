#include "history.h"
#include <stdlib.h>
#include <string.h>

history_node_t* history_node_create(const char *url, const char *title) {
    history_node_t *node = calloc(1, sizeof(history_node_t));
    if (node) {
        node->url = strdup(url);
        node->title = title ? strdup(title) : strdup(url);
        node->favicon_data = NULL;
        node->favicon_size = 0;
    }
    return node;
}

void history_node_set_favicon(history_node_t *node, void *data, size_t size) {
    if (!node) return;
    if (node->favicon_data) free(node->favicon_data);
    node->favicon_data = data;
    node->favicon_size = size;
}

history_tree_t* history_create() {
    return calloc(1, sizeof(history_tree_t));
}

void history_add(history_tree_t *tree, const char *url, const char *title) {
    if (!tree) return;
    history_node_t *node = history_node_create(url, title);
    
    if (!tree->root) {
        tree->root = node;
    } else {
        history_node_t *curr = tree->current;
        node->parent = curr;
        if (curr->children_count == curr->children_capacity) {
            curr->children_capacity = curr->children_capacity == 0 ? 4 : curr->children_capacity * 2;
            curr->children = realloc(curr->children, sizeof(history_node_t*) * curr->children_capacity);
        }
        curr->children[curr->children_count++] = node;
    }
    tree->current = node;
}

void history_node_free(history_node_t *node) {
    if (!node) return;
    for (int i = 0; i < node->children_count; i++) {
        history_node_free(node->children[i]);
    }
    free(node->url);
    free(node->title);
    if (node->favicon_data) free(node->favicon_data);
    free(node->children);
    free(node);
}

void history_free(history_tree_t *tree) {
    if (!tree) return;
    history_node_free(tree->root);
    free(tree);
}

// Check if target_url matches any child URL of current node
int history_is_new_branch(history_node_t *current, const char *target_url) {
    if (!current || !target_url) return 1;

    for (int i = 0; i < current->children_count; i++) {
        if (current->children[i] && current->children[i]->url &&
            strcmp(current->children[i]->url, target_url) == 0) {
            return 0;  // Found matching child - not a new branch
        }
    }
    return 1;  // No matching child - this is a new branch
}

// Navigate to existing history node and create branch if needed
void history_navigate_to(history_tree_t *tree, history_node_t *target, const char *url, const char *title) {
    if (!tree || !target) return;

    tree->current = target;
    // If navigating forward from target (adding new content), it will be added as child via history_add
}

// Reset history: create a new root for completely new URL (manual navigation)
void history_reset(history_tree_t *tree, const char *url, const char *title) {
    if (!tree) return;

    // Create a new root
    history_node_t *new_root = history_node_create(url, title);

    // Preserve the old tree as a child of the new root (creates parallel branch above)
    if (tree->root) {
        if (new_root->children_capacity == 0) {
            new_root->children_capacity = 4;
            new_root->children = malloc(sizeof(history_node_t*) * new_root->children_capacity);
        }
        // Add old root as a child of new root (old tree becomes a branch)
        new_root->children[new_root->children_count++] = tree->root;
        tree->root->parent = new_root;
    }

    tree->root = new_root;
    tree->current = new_root;
}
