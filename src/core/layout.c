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

// Recursive function to calculate layout
// x, y: Position of the content area of the PARENT
// width: Width of the content area of the PARENT
void layout_calculate_positions(layout_box_t *box, int x, int y, int width) {
    if (!box || !box->node || !box->node->style) return;
    if (box->node->style->display == DISPLAY_NONE) return;

    // 1. Calculate Horizontal Dimensions (Width, Margins, Padding, Borders)
    
    // Horizontal Margins, Borders, Padding
    int margin_left = box->node->style->margin_left;
    int margin_right = box->node->style->margin_right;
    int padding_left = box->node->style->padding_left;
    int padding_right = box->node->style->padding_right;
    int border = box->node->style->border_width;

    // Box X Position (Border Box Left Edge)
    // Relative to Parent Content X (x argument)
    box->dimensions.x = x + margin_left;
    
    // Content Width
    int content_width;
    if (box->node->style->width > 0) {
        content_width = box->node->style->width;
    } else {
        // Auto width for blocks: fill available parent content width minus our own spacers
        content_width = width - (margin_left + margin_right + padding_left + padding_right + (border * 2));
    }
    if (content_width < 0) content_width = 0;

    // Handle Inline specific width (shrink to fit text/content) - Simplified
    if (box->node->style->display == DISPLAY_INLINE || box->node->type == DOM_NODE_TEXT) {
         if (box->node->type == DOM_NODE_TEXT && box->node->content) {
             content_width = strlen(box->node->content) * 8; // Approx char width
         } else if (box->node->type == DOM_NODE_ELEMENT && box->node->tag_name && strcasecmp(box->node->tag_name, "img") == 0) {
             content_width = 100; // Placeholder image width
             if (box->node->image_size > 0) {
                 // If we had image metadata (width/height) loaded, we'd use it here.
                 // For now, assume 100 if loaded.
             }
         }
    }

    // 2. Calculate Vertical Position (Top Edge)
    // y argument is the current cursor Y in the parent's content area.
    int margin_top = box->node->style->margin_top;
    int margin_bottom = box->node->style->margin_bottom;
    int padding_top = box->node->style->padding_top;
    int padding_bottom = box->node->style->padding_bottom;

    box->dimensions.y = y + margin_top;

    // 3. Layout Children (Recursion)
    
    // Children start position (Parent's Content Box)
    int child_start_x = box->dimensions.x + border + padding_left;
    int child_start_y = box->dimensions.y + border + padding_top;
    
    int current_child_x = child_start_x;
    int current_child_y = child_start_y;
    int line_height = 0; // For inline wrapping

    // Table special case
    int cell_count = 0;
    if (box->node->style->display == DISPLAY_TABLE_ROW) {
        layout_box_t *c = box->first_child;
        while (c) {
            if (c->node->style->display == DISPLAY_TABLE_CELL) cell_count++;
            c = c->next_sibling;
        }
    }

    layout_box_t *child = box->first_child;
    while (child) {
        // Table Cell Layout
        if (box->node->style->display == DISPLAY_TABLE_ROW && child->node->style->display == DISPLAY_TABLE_CELL) {
             int cell_w = content_width / (cell_count > 0 ? cell_count : 1);
             // Table cells force a specific width
             child->node->style->width = cell_w; // Hacky overwrite for simplicity
             layout_calculate_positions(child, current_child_x, current_child_y, cell_w); // Pass parent width as context? No, strictly cell width.
             
             // Manually adjust width because recursive call might set it to auto relative to parent which is fine, 
             // but we want strict columns.
             child->dimensions.width = cell_w; // Force it.
             
             current_child_x += cell_w;
             // Height will be max height of row, handled later? 
             // For now simple top alignment.
             if (child->dimensions.height > line_height) line_height = child->dimensions.height;
        }
        // Inline Layout
        else if (child->node->style->display == DISPLAY_INLINE || child->node->type == DOM_NODE_TEXT) {
            layout_calculate_positions(child, current_child_x, current_child_y, content_width - (current_child_x - child_start_x));
            
            // Check for wrap
            if (current_child_x + child->dimensions.width > child_start_x + content_width) {
                 current_child_x = child_start_x;
                 current_child_y += line_height;
                 line_height = 0;
                 // Recalculate pos at new line
                 layout_calculate_positions(child, current_child_x, current_child_y, content_width);
            }

            child->dimensions.x = current_child_x; // Ensure strict placement
            child->dimensions.y = current_child_y;

            current_child_x += child->dimensions.width;
            if (child->dimensions.height > line_height) line_height = child->dimensions.height;
        } 
        // Block Layout
        else {
            if (line_height > 0) {
                current_child_y += line_height;
                line_height = 0;
            }
            current_child_x = child_start_x; // Reset X for block
            
            layout_calculate_positions(child, current_child_x, current_child_y, content_width);
            
            current_child_y = child->dimensions.y + child->dimensions.height + child->node->style->margin_bottom; // child y includes margin_top already
            // Ensure we account for the child's full outer box in the parent's vertical flow
        }
        child = child->next_sibling;
    }

    // Flush last line
    if (line_height > 0) {
        current_child_y += line_height;
    }
    
    // For table row, set height to max cell height
    if (box->node->style->display == DISPLAY_TABLE_ROW) {
        // Adjust all children height to max?
        // Skip for now.
        current_child_y = child_start_y + line_height;
    }

    // 4. Calculate Final Height (Height + Padding + Border)
    int content_height = current_child_y - child_start_y;
    
    // Min height for text/images
    if (content_height == 0) {
        if (box->node->type == DOM_NODE_TEXT) content_height = 20; 
        if (box->node->tag_name && strcasecmp(box->node->tag_name, "img") == 0) content_height = 100;
    }

    if (box->node->style->height > 0) {
        content_height = box->node->style->height;
    }

    // Final Box Dimensions (Border Box)
    box->dimensions.width = content_width + padding_left + padding_right + (border * 2);
    box->dimensions.height = content_height + padding_top + padding_bottom + (border * 2);
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
    // Root starts at (0,0) with container width
    if (root->tag_name && strcmp(root->tag_name, "root") == 0) {
        layout_calculate_positions(box, 0, 0, container_width);
    }

    return box;
}