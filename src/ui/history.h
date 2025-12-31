#ifndef HISTORY_H
#define HISTORY_H

#include <stddef.h>

typedef struct history_node_s {
    char *url;
    char *title;
    void *favicon_data;
    size_t favicon_size;
    struct history_node_s *parent;
    struct history_node_s **children;
    int children_count;
    int children_capacity;
} history_node_t;

typedef struct {
    history_node_t *root;
    history_node_t *current;
} history_tree_t;

history_tree_t* history_create();
void history_add(history_tree_t *tree, const char *url, const char *title);
void history_node_set_favicon(history_node_t *node, void *data, size_t size);
void history_free(history_tree_t *tree);

// Check if navigating from current node to target_url would create a new branch
// Returns 1 if target_url is not the URL of any child of current, 0 otherwise
int history_is_new_branch(history_node_t *current, const char *target_url);

// Navigate to an existing history node and handle branching if navigating from elsewhere
void history_navigate_to(history_tree_t *tree, history_node_t *target, const char *url, const char *title);

// Create a new root node (for manual navigation to completely new URL)
void history_reset(history_tree_t *tree, const char *url, const char *title);

#endif // HISTORY_H
