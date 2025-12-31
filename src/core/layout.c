#include "layout.h"
#include <stdlib.h>
#include <string.h>

layout_box_t* layout_create_box(node_t *node) {
    layout_box_t *box = calloc(1, sizeof(layout_box_t));
    if (box) {
        box->node = node;
    }
    return box;
}

void layout_free(layout_box_t *box) {
    if (!box) return;
    layout_box_t *child = box->first_child;
    while (child) {
        layout_box_t *next = child->next_sibling;
        layout_free(child);
        child = next;
    }
    free(box);
}

void layout_calculate_positions(layout_box_t *box, int x, int y, int width) {
    box->dimensions.x = x + box->node->style->margin_left + box->node->style->padding_left;
    box->dimensions.y = y + box->node->style->margin_top + box->node->style->padding_top;
    box->dimensions.width = width - (box->node->style->margin_left + box->node->style->margin_right + 
                                     box->node->style->padding_left + box->node->style->padding_right);

    int current_y = box->dimensions.y;
    int max_h = 0;

    layout_box_t *child = box->first_child;
    while (child) {
        // Very simple: treat everything as block for now
        layout_calculate_positions(child, box->dimensions.x, current_y, box->dimensions.width);
        current_y += child->dimensions.height + child->node->style->margin_top + child->node->style->margin_bottom;
        child = child->next_sibling;
    }

    if (box->node->style->height > 0) {
        box->dimensions.height = box->node->style->height;
    } else {
        box->dimensions.height = current_y - box->dimensions.y;
        if (box->dimensions.height < 20 && box->node->type == NODE_TEXT) {
             box->dimensions.height = 20; // Default height for text
        }
    }
}

layout_box_t* layout_create_tree(node_t *root, int container_width) {
    if (!root) return NULL;

    layout_box_t *box = layout_create_box(root);
    
    node_t *child_node = root->first_child;
    layout_box_t *last_child_box = NULL;

    while (child_node) {
        layout_box_t *child_box = layout_create_tree(child_node, container_width);
        if (!box->first_child) {
            box->first_child = child_box;
        } else {
            last_child_box->next_sibling = child_box;
        }
        last_child_box = child_box;
        child_node = child_node->next_sibling;
    }

    // After building tree, calculate positions from top down
    if (root->tag_name && strcmp(root->tag_name, "root") == 0) {
        layout_calculate_positions(box, 0, 0, container_width);
    }

    return box;
}
