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

static int is_block(node_t *node) {
    if (!node || !node->style) return 0;
    return (node->style->display == DISPLAY_BLOCK || 
            node->style->display == DISPLAY_TABLE || 
            node->style->display == DISPLAY_TABLE_ROW ||
            node->style->display == DISPLAY_TABLE_CELL);
}

static int get_inline_width(node_t *node) {
    if (node->type == DOM_NODE_TEXT && node->content) {
        return strlen(node->content) * 8; 
    }
    if (node->type == DOM_NODE_ELEMENT && node->tag_name && strcasecmp(node->tag_name, "img") == 0) {
        return 100; 
    }
    return 0;
}

// Inline Line Buffer
#define MAX_LINE_ITEMS 128
typedef struct {
    layout_box_t *items[MAX_LINE_ITEMS];
    int count;
    int current_width;
    int max_height;
} line_buffer_t;

static void flush_line(line_buffer_t *line, int x_start, int y, int total_width, text_align_t align) {
    if (line->count == 0) return;

    int free_space = total_width - line->current_width;
    int start_offset = 0;

    if (align == TEXT_ALIGN_CENTER) start_offset = free_space / 2;
    else if (align == TEXT_ALIGN_RIGHT) start_offset = free_space;
    
    if (start_offset < 0) start_offset = 0; // Don't shift left out of box

    int current_x = x_start + start_offset;
    
    for (int i = 0; i < line->count; i++) {
        layout_box_t *box = line->items[i];
        box->dimensions.x = current_x;
        box->dimensions.y = y + (line->max_height - box->dimensions.height); // Bottom align roughly
        // Better: Top align
        box->dimensions.y = y; 
        
        current_x += box->dimensions.width;
    }

    // Reset line
    line->count = 0;
    line->current_width = 0;
    line->max_height = 0;
}

rect_t layout_compute_internal(layout_box_t *box, int x, int y, int available_width);

void layout_compute_internal_void(layout_box_t *box, int x, int y, int available_width) {
    layout_compute_internal(box, x, y, available_width);
}

rect_t layout_compute_internal(layout_box_t *box, int x, int y, int available_width) {
    rect_t used_dim = {x, y, 0, 0};
    if (!box || !box->node || !box->node->style) return used_dim;
    if (box->node->style->display == DISPLAY_NONE) return used_dim;

    style_t *style = box->node->style;
    
    int mt = style->margin_top;
    int mb = style->margin_bottom;
    int ml = style->margin_left;
    int mr = style->margin_right;
    int pt = style->padding_top;
    int pb = style->padding_bottom;
    int pl = style->padding_left;
    int pr = style->padding_right;
    int bw = style->border_width;

    box->dimensions.x = x + ml;
    box->dimensions.y = y + mt;

    int content_available_width = available_width - (ml + mr + pl + pr + (bw * 2));
    if (content_available_width < 0) content_available_width = 0;

    if (is_block(box->node)) {
        if (style->width > 0) box->dimensions.width = style->width;
        else box->dimensions.width = content_available_width;
    } else {
        box->dimensions.width = get_inline_width(box->node);
    }

    int child_x_start = box->dimensions.x + bw + pl;
    int cursor_y = box->dimensions.y + bw + pt;
    int max_child_w = box->dimensions.width - (pl + pr + bw*2); // Inner content width of box
    if (max_child_w < 0) max_child_w = 0;

    // Line buffering for inline flow
    line_buffer_t line = {0};

    // Table Handling
    int cell_count = 0;
    if (style->display == DISPLAY_TABLE_ROW) {
        layout_box_t *c = box->first_child;
        while (c) { if (c->node->style->display == DISPLAY_TABLE_CELL) cell_count++; c = c->next_sibling; }
    }

    layout_box_t *child = box->first_child;
    while (child) {
        if (style->display == DISPLAY_TABLE_ROW && child->node->style->display == DISPLAY_TABLE_CELL) {
            // ... Table cell logic (omitted for brevity, assume block-like grid) ...
            // Re-implement basic table grid
             int cell_w = box->dimensions.width / (cell_count > 0 ? cell_count : 1);
             child->node->style->width = cell_w; // Force style width
             layout_compute_internal_void(child, child_x_start, cursor_y, cell_w); 
             child->dimensions.width = cell_w; // Force dim width
             child_x_start += cell_w; // Move cursor right directly
             
             if (child->dimensions.height > line.max_height) line.max_height = child->dimensions.height;
        } 
        else if (is_block(child->node)) {
            // Flush any inline content first
            if (line.count > 0) {
                int lh = line.max_height;
                flush_line(&line, child_x_start, cursor_y, max_child_w, style->text_align);
                cursor_y += lh;
            }

            // Layout Block
            rect_t res = layout_compute_internal(child, child_x_start, cursor_y, max_child_w);
            cursor_y += res.height + child->node->style->margin_top + child->node->style->margin_bottom;
        } 
        else {
            // Inline
            // 1. Calculate size tentatively (pass full width to let it size itself)
            // Note: Inline containers need to know available space to wrap their own children?
            // For now, simplify: Measure intrinsic size
            
            rect_t res = layout_compute_internal(child, 0, 0, max_child_w); 
            // Pos is 0,0 because we don't know it yet. We just want dims.
            
            int child_w = res.width + child->node->style->margin_left + child->node->style->margin_right;
            int child_h = res.height + child->node->style->margin_top + child->node->style->margin_bottom;

            // Check fit
            if (line.current_width + child_w > max_child_w && line.count > 0) {
                // Wrap
                int lh = line.max_height;
                flush_line(&line, child_x_start, cursor_y, max_child_w, style->text_align);
                cursor_y += lh; 
                // New line starts with 0 height
            }

            // Add to line
            if (line.count < MAX_LINE_ITEMS) {
                line.items[line.count++] = child;
                line.current_width += child_w;
                if (child_h > line.max_height) line.max_height = child_h;
            } else {
                // Buffer full, force flush (rare edge case)
                int lh = line.max_height;
                flush_line(&line, child_x_start, cursor_y, max_child_w, style->text_align);
                cursor_y += lh;
                
                // Add current
                line.items[line.count++] = child;
                line.current_width += child_w;
                line.max_height = child_h;
            }
            
            // If inline container grew (e.g. bold text), propagate? 
            // We used recursive layout call so 'child' dimensions are set. 
            // flush_line will just move them.
        }
        child = child->next_sibling;
    }

    // Flush remaining
    if (line.count > 0) {
        int lh = line.max_height;
        flush_line(&line, child_x_start, cursor_y, max_child_w, style->text_align);
        cursor_y += lh;
    }

    // Calc height
    int content_height = cursor_y - (box->dimensions.y + bw + pt);
    if (style->height > 0) box->dimensions.height = style->height;
    else {
        box->dimensions.height = content_height;
         if (box->dimensions.height == 0 && box->node->type == DOM_NODE_TEXT) box->dimensions.height = 20;
    }
    
    // Adjust height for Table Row
    if (style->display == DISPLAY_TABLE_ROW) box->dimensions.height = line.max_height > 0 ? line.max_height : 20;

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