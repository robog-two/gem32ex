#include "layout.h"
#include "platform.h"
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
    display_t d = node->style->display;
    return (d == DISPLAY_BLOCK || d == DISPLAY_TABLE || d == DISPLAY_TABLE_ROW || d == DISPLAY_TABLE_CELL);
}

static int is_inline(node_t *node) {
    if (!node || !node->style) return 1;
    return node->style->display == DISPLAY_INLINE;
}

// --- Inline Layout Algorithm (Fragment-based Line Buffering) ---

#define MAX_LINE_FRAGMENTS 256
typedef struct {
    layout_box_t *items[MAX_LINE_FRAGMENTS];
    int count;
    int width;
    int height;
    int baseline;
} line_info_t;

static void flush_line(line_info_t *line, int x_offset, int *y_cursor, int available_width, text_align_t align) {
    if (line->count == 0) return;

    int free_space = available_width - line->width;
    int x_start = x_offset;
    if (align == TEXT_ALIGN_CENTER) x_start += free_space / 2;
    else if (align == TEXT_ALIGN_RIGHT) x_start += free_space;

    if (x_start < x_offset) x_start = x_offset;

    for (int i = 0; i < line->count; i++) {
        layout_box_t *child = line->items[i];
        child->fragment.border_box.x = x_start;
        // Simple vertical alignment: top (should be baseline, but simple for now)
        child->fragment.border_box.y = *y_cursor; 
        
        x_start += child->fragment.border_box.width;
    }

    *y_cursor += line->height;
    line->count = 0;
    line->width = 0;
    line->height = 0;
}

static void layout_inline_children(layout_box_t *box, constraint_space_t space, int *y_cursor) {
    line_info_t line = {0};
    int x_start = box->fragment.content_box.x;
    int available_width = box->fragment.content_box.width;
    if (available_width <= 0) available_width = space.available_width; // Fallback
    
    layout_box_t *child = box->first_child;
    while (child) {
        if (child->node->style->display == DISPLAY_NONE) {
            child = child->next_sibling;
            continue;
        }

        // 1. Measure child
        // Inline children usually measure intrinsically, unless constrained (like wrapped text)
        constraint_space_t child_space = space;
        child_space.available_width = available_width;
        
        int child_w = 0;
        int child_h = 0;

        if (child->node->type == DOM_NODE_TEXT && child->node->content) {
            // Check if it fits as a single line first
            int w, h;
            platform_measure_text(child->node->content, child->node->style, -1, &w, &h);
            
            if (line.width + w > available_width && line.count > 0) {
                // Won't fit on current line, flush current line
                flush_line(&line, x_start, y_cursor, available_width, box->node->style->text_align);
            }
            
            // Now check if it fits on a fresh line
            if (w > available_width) {
                // Too big for a single line, must wrap internally
                platform_measure_text(child->node->content, child->node->style, available_width, &w, &h);
            }
            
            child_w = w;
            child_h = h;
            child->fragment.border_box.width = w;
            child->fragment.border_box.height = h;
        } else {
            // Recursively layout element (e.g. <img>, <b>, <span>)
            layout_compute(child, child_space);
            child_w = child->fragment.border_box.width;
            child_h = child->fragment.border_box.height;

             // Logic to flush if element doesn't fit
            if (line.width + child_w > available_width && line.count > 0) {
                flush_line(&line, x_start, y_cursor, available_width, box->node->style->text_align);
            }
        }

        // Add to line
        if (line.count < MAX_LINE_FRAGMENTS) {
            line.items[line.count++] = child;
            line.width += child_w;
            if (child_h > line.height) line.height = child_h;
        }

        // If even after wrapping internally, it still consumes the full line (or more),
        // we essentially just flushed before adding it, and now we might need to flush after?
        // But flush_line logic just positions items.
        // If 'child' is now effectively a block (wrapped text), it sits on this line.
        // If another item comes, it will check "line.width + next_w > available".
        // Since child_w is likely == available_width, it will force next item to new line.
        // This is correct behavior for "Atomic Inline Block" text wrapping.

        child = child->next_sibling;
    }

    flush_line(&line, x_start, y_cursor, available_width, box->node->style->text_align);
}

// --- Block Layout Algorithm ---

void layout_compute(layout_box_t *box, constraint_space_t space) {
    if (!box || !box->node || !box->node->style) return;
    style_t *style = box->node->style;
    if (style->display == DISPLAY_NONE) return;

    box->last_space = space;

    // 1. Resolve Box Model
    int bw = style->border_width;
    int pl = style->padding_left, pr = style->padding_right, pt = style->padding_top, pb = style->padding_bottom;
    int ml = style->margin_left, mr = style->margin_right, mt = style->margin_top, mb = style->margin_bottom;

    // Position (relative to parent content box, set by parent algorithm)
    // We expect box->fragment.border_box.x/y to be hinted by parent if it's block
    // but usually parent just gives available space and we return height.

    // Calculate width
    if (style->display == DISPLAY_BLOCK || style->display == DISPLAY_TABLE ||
        style->display == DISPLAY_TABLE_ROW || style->display == DISPLAY_TABLE_CELL) {
        if (style->width > 0) box->fragment.border_box.width = style->width + (bw * 2) + pl + pr;
        else box->fragment.border_box.width = space.available_width;
    } else {
        // Inline-level (non-text inline-block or similar)
        // For text, width is handled in layout_inline_children.
        // For images or others:
        if (box->node->type == DOM_NODE_ELEMENT && box->node->tag_name && strcasecmp(box->node->tag_name, "img") == 0) {
             box->fragment.border_box.width = (style->width > 0) ? style->width : 100;
        } else {
             box->fragment.border_box.width = 0; // Or measure content?
        }
    }

    // Content area
    box->fragment.content_box.width = box->fragment.border_box.width - (bw * 2 + pl + pr);
    if (box->fragment.content_box.width < 0) box->fragment.content_box.width = 0;
    
    // Y-cursor for children
    int child_y = box->fragment.border_box.y + bw + pt;
    box->fragment.content_box.x = box->fragment.border_box.x + bw + pl;
    box->fragment.content_box.y = child_y;

    // 2. Layout Children
    if (style->display == DISPLAY_TABLE_ROW) {
        // Table row layout: Distribute width among cells
        int cell_count = 0;
        layout_box_t *c = box->first_child;
        while (c) { if (c->node->style->display == DISPLAY_TABLE_CELL) cell_count++; c = c->next_sibling; }
        
        if (cell_count > 0) {
            int cell_w = box->fragment.content_box.width / cell_count;
            int x_offset = box->fragment.content_box.x;
            int max_h = 0;
            layout_box_t *cell = box->first_child;
            while (cell) {
                if (cell->node->style->display == DISPLAY_TABLE_CELL) {
                    constraint_space_t cell_space = space;
                    cell_space.available_width = cell_w;
                    cell_space.is_fixed_width = 1;
                    cell->fragment.border_box.x = x_offset;
                    cell->fragment.border_box.y = box->fragment.content_box.y;
                    layout_compute(cell, cell_space);
                    if (cell->fragment.border_box.height > max_h) max_h = cell->fragment.border_box.height;
                    x_offset += cell_w;
                }
                cell = cell->next_sibling;
            }
            child_y += max_h;
        }
    } else {
        int has_block_children = 0;
        layout_box_t *c = box->first_child;
        while (c) { if (is_block(c->node)) { has_block_children = 1; break; } c = c->next_sibling; }

        if (has_block_children) {
            // Block flow
            layout_box_t *child = box->first_child;
            int prev_margin_bottom = 0;

            while (child) {
                if (child->node->style->display == DISPLAY_NONE) {
                    child = child->next_sibling;
                    continue;
                }

                int collapse = 0;
                if (is_block(child->node)) {
                    int cur_mt = child->node->style->margin_top;
                    collapse = (cur_mt > prev_margin_bottom) ? cur_mt : prev_margin_bottom;
                    if (child == box->first_child) collapse = cur_mt;
                    child_y += collapse;
                }

                constraint_space_t child_space = space;
                child_space.available_width = box->fragment.content_box.width;
                if (is_block(child->node)) {
                    child->fragment.border_box.x = box->fragment.content_box.x + child->node->style->margin_left;
                    child_space.available_width -= (child->node->style->margin_left + child->node->style->margin_right);
                } else {
                    child->fragment.border_box.x = box->fragment.content_box.x;
                }
                child->fragment.border_box.y = child_y;

                layout_compute(child, child_space);

                child_y += child->fragment.border_box.height;
                if (is_block(child->node)) {
                    prev_margin_bottom = child->node->style->margin_bottom;
                }
                child = child->next_sibling;
            }
            child_y += prev_margin_bottom;
        } else {
            // Inline flow
            layout_inline_children(box, space, &child_y);
        }
    }

    // 3. Resolve Height
    int content_height = child_y - (box->fragment.border_box.y + bw + pt);
    if (style->height > 0) box->fragment.border_box.height = style->height + (bw * 2) + pt + pb;
    else {
        box->fragment.border_box.height = content_height + (bw * 2) + pt + pb;
        // Text node minimum height
        if (box->node->type == DOM_NODE_TEXT && box->fragment.border_box.height == 0) {
             box->fragment.border_box.height = 16;
        }
    }
    
    box->fragment.content_box.height = content_height;
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

    // Top-level layout trigger
    if (root->tag_name && strcmp(root->tag_name, "root") == 0) {
        constraint_space_t space = {container_width, 0, 1, 0};
        box->fragment.border_box.x = 0;
        box->fragment.border_box.y = 0;
        layout_compute(box, space);
    }

    return box;
}
