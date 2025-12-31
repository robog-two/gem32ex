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
