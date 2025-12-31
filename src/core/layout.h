#ifndef LAYOUT_H
#define LAYOUT_H

#include "dom.h"

typedef struct rect_s {
    int x, y, width, height;
} rect_t;

typedef struct layout_box_s {
    node_t *node;
    rect_t dimensions;
    rect_t padding;
    rect_t border;
    rect_t margin;
    
    struct layout_box_s *first_child;
    struct layout_box_s *next_sibling;
} layout_box_t;

layout_box_t* layout_create_tree(node_t *root, int container_width);
void layout_free(layout_box_t *box);

#endif // LAYOUT_H
