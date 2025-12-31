#include "layout.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

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

// Helper to check if a node is block-level
static int is_block(node_t *node) {
    if (!node || !node->style) return 0;
    return (node->style->display == DISPLAY_BLOCK || 
            node->style->display == DISPLAY_TABLE || 
            node->style->display == DISPLAY_TABLE_ROW ||
            node->style->display == DISPLAY_TABLE_CELL); // Cells behave like blocks inside rows
}

// Helper to get preferred width of an inline leaf (text/img)
static int get_inline_width(node_t *node) {
    if (node->type == DOM_NODE_TEXT && node->content) {
        return strlen(node->content) * 8; // Approx 8px per char
    }
    if (node->type == DOM_NODE_ELEMENT && node->tag_name && strcasecmp(node->tag_name, "img") == 0) {
        return 100; // Placeholder
    }
    return 0;
}

// Compute layout for a box given its position and max available width.
// Returns the dimensions (w, h) effectively occupied by this box.
rect_t layout_compute_internal(layout_box_t *box, int x, int y, int available_width) {
    rect_t used_dim = {x, y, 0, 0};
    if (!box || !box->node || !box->node->style) return used_dim;
    if (box->node->style->display == DISPLAY_NONE) return used_dim;

    style_t *style = box->node->style;
    
    // 1. Determine Box Model Metrics
    int mt = style->margin_top;
    int mb = style->margin_bottom;
    int ml = style->margin_left;
    int mr = style->margin_right;
    int pt = style->padding_top;
    int pb = style->padding_bottom;
    int pl = style->padding_left;
    int pr = style->padding_right;
    int bw = style->border_width;

    // 2. Determine Own Dimensions (Outer)
    // Position is absolute
    box->dimensions.x = x + ml;
    box->dimensions.y = y + mt;

    int content_available_width = available_width - (ml + mr + pl + pr + (bw * 2));
    if (content_available_width < 0) content_available_width = 0;

    // Set Content Width
    if (is_block(box->node)) {
        // Block: Fill width
        if (style->width > 0) box->dimensions.width = style->width;
        else box->dimensions.width = content_available_width;
    } else {
        // Inline: Width is 0 initially, grows with content
        box->dimensions.width = get_inline_width(box->node);
    }

    // 3. Layout Children
    int child_x = box->dimensions.x + bw + pl;
    int child_y = box->dimensions.y + bw + pt;
    int max_child_w = available_width - (ml + mr); // Constraint for children wrapping
    if (max_child_w < 0) max_child_w = 0;

    int cursor_x = child_x;
    int cursor_y = child_y;
    int line_height = 0;

    // Table Handling (Simple)
    int cell_count = 0;
    if (style->display == DISPLAY_TABLE_ROW) {
        layout_box_t *c = box->first_child;
        while (c) { if (c->node->style->display == DISPLAY_TABLE_CELL) cell_count++; c = c->next_sibling; }
    }

    layout_box_t *child = box->first_child;
    while (child) {
        if (style->display == DISPLAY_TABLE_ROW && child->node->style->display == DISPLAY_TABLE_CELL) {
            // Table Cell: Fixed width column
            int cell_w = box->dimensions.width / (cell_count > 0 ? cell_count : 1);
            // Cells act like blocks in a fixed grid
            layout_compute_internal(child, cursor_x, child_y, cell_w); 
            // Force width
            child->dimensions.width = cell_w - (child->node->style->margin_left + child->node->style->margin_right); 
            cursor_x += cell_w;
            if (child->dimensions.height + child->node->style->margin_top + child->node->style->margin_bottom > line_height)
                line_height = child->dimensions.height + child->node->style->margin_top + child->node->style->margin_bottom;
        } 
        else if (is_block(child->node)) {
            // Block Child: Clears line
            if (line_height > 0) {
                cursor_y += line_height;
                line_height = 0;
            }
            cursor_x = child_x; // Reset X
            
            // Recurse
            rect_t child_res = layout_compute_internal(child, cursor_x, cursor_y, box->dimensions.width); // Blocks get parent's content width
            
            cursor_y += child_res.height + child->node->style->margin_top + child->node->style->margin_bottom;
        } 
        else {
            // Inline Child
            // We need to tentatively check if it fits. 
            // For simple recursion, we assume it fits, calc size, then check wrap.
            
            // Pass remaining width on line? No, pass full width but we check position.
            // If we pass full content_width, an inline-block might take it all.
            // Inline elements generally "shrink wrap".
            
            int remaining_on_line = (child_x + box->dimensions.width) - cursor_x; 
            
            // Recurse
            rect_t child_res = layout_compute_internal(child, cursor_x, cursor_y, box->dimensions.width); // Give full context width
            
            // Check wrap
            int child_outer_w = child->dimensions.width + child->node->style->margin_left + child->node->style->margin_right;
            int child_outer_h = child->dimensions.height + child->node->style->margin_top + child->node->style->margin_bottom;

            if (cursor_x + child_outer_w > child_x + box->dimensions.width && cursor_x > child_x) {
                // Wrap
                cursor_x = child_x;
                cursor_y += line_height;
                line_height = 0;
                
                // Re-layout at new position
                child_res = layout_compute_internal(child, cursor_x, cursor_y, box->dimensions.width);
                child_outer_w = child->dimensions.width + child->node->style->margin_left + child->node->style->margin_right;
                child_outer_h = child->dimensions.height + child->node->style->margin_top + child->node->style->margin_bottom;
            }

            cursor_x += child_outer_w;
            if (child_outer_h > line_height) line_height = child_outer_h;
            
            // Propagate size up if we are an inline container
            if (!is_block(box->node) && cursor_x - child_x > box->dimensions.width) {
                box->dimensions.width = cursor_x - child_x;
            }
        }
        child = child->next_sibling;
    }

    // Finish last line
    if (line_height > 0) cursor_y += line_height;

    // 4. Calculate Height
    int content_height = cursor_y - child_y;
    if (style->height > 0) {
        box->dimensions.height = style->height;
    } else {
        box->dimensions.height = content_height;
        // Min height for text/images if empty
        if (box->dimensions.height == 0) {
             if (box->node->type == DOM_NODE_TEXT) box->dimensions.height = 20;
             if (box->node->tag_name && strcasecmp(box->node->tag_name, "img") == 0) box->dimensions.height = 100;
        }
    }
    
    // For tables rows
    if (style->display == DISPLAY_TABLE_ROW) box->dimensions.height = line_height;

    used_dim.width = box->dimensions.width + ml + mr + pl + pr + (bw*2);
    used_dim.height = box->dimensions.height + mt + mb + pt + pb + (bw*2);
    
    return used_dim;
}

void layout_compute(layout_box_t *box, int x, int y, int available_width) {
    layout_compute_internal(box, x, y, available_width);
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

    if (root->tag_name && strcmp(root->tag_name, "root") == 0) {
        layout_compute(box, 0, 0, container_width);
    }

    return box;
}
