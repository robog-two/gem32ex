#ifndef HISTORY_H
#define HISTORY_H

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

#endif // HISTORY_H
