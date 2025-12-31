#ifndef LAYOUT_H
#define LAYOUT_H

#include "dom.h"

typedef struct rect_s {
    int x, y, width, height;
} rect_t;

// LayoutNG-inspired: Input constraints for layout
typedef struct {
    int available_width;
    int available_height;
    int is_fixed_width;
    int is_fixed_height;
} constraint_space_t;

// LayoutNG-inspired: The output of a layout pass
typedef struct {
    rect_t border_box;   // Position and size including borders
    rect_t content_box;  // Inner content area
    int baseline;        // Distance from top of box to text baseline
} physical_fragment_t;

typedef struct layout_box_s {
    node_t *node;
    physical_fragment_t fragment;
    
    // Cached constraints (for future invalidation logic)
    constraint_space_t last_space;

    struct layout_box_s *parent;
    struct layout_box_s *first_child;
    struct layout_box_s *next_sibling;
} layout_box_t;

layout_box_t* layout_create_tree(node_t *root, int container_width);
void layout_free(layout_box_t *box);

// Modernized layout entry point
void layout_compute(layout_box_t *box, constraint_space_t space);

layout_box_t* layout_hit_test(layout_box_t *root, int x, int y);

#endif // LAYOUT_H
