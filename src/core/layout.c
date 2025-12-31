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

// --- Inline Layout Algorithm (Baseline Aligned) ---

#define MAX_LINE_FRAGMENTS 256
typedef struct {
    layout_box_t *items[MAX_LINE_FRAGMENTS];
    int count;
    int width;
    int height;
    int max_ascent;
    int max_descent;
} line_info_t;

static void flush_line(line_info_t *line, int x_offset, int *y_cursor, int available_width, text_align_t align) {
    if (line->count == 0) return;

    int free_space = available_width - line->width;
    int x_start = x_offset;
    if (align == TEXT_ALIGN_CENTER) x_start += free_space / 2;
    else if (align == TEXT_ALIGN_RIGHT) x_start += free_space;

    if (x_start < x_offset) x_start = x_offset;

    int line_height = line->max_ascent + line->max_descent;
    int baseline_y = *y_cursor + line->max_ascent;

    for (int i = 0; i < line->count; i++) {
        layout_box_t *child = line->items[i];
        child->fragment.border_box.x = x_start;
        
        // Align to baseline
        if (child->node->type == DOM_NODE_TEXT) {
            child->fragment.border_box.y = baseline_y - child->fragment.baseline;
        } else {
            // For blocks/images in inline flow, bottom align to baseline by default
            child->fragment.border_box.y = baseline_y - child->fragment.border_box.height;
        }
        
        x_start += child->fragment.border_box.width;
    }

    *y_cursor += line_height;
    line->count = 0;
    line->width = 0;
    line->max_ascent = 0;
    line->max_descent = 0;
}

static void layout_inline_children(layout_box_t *box, constraint_space_t space, int *y_cursor) {
    line_info_t line = {0};
    int x_start = box->fragment.content_box.x;
    int available_width = box->fragment.content_box.width;
    
    layout_box_t *child = box->first_child;
    while (child) {
        if (child->node->style->display == DISPLAY_NONE) {
            child = child->next_sibling;
            continue;
        }

        int child_w = 0, child_h = 0, child_ascent = 0, child_descent = 0;

        if (child->node->type == DOM_NODE_TEXT && child->node->content) {
            int w, h, baseline;
            platform_measure_text(child->node->content, child->node->style, -1, &w, &h, &baseline);
            
            if (line.width + w > available_width && line.count > 0) {
                flush_line(&line, x_start, y_cursor, available_width, box->node->style->text_align);
            }
            
            if (w > available_width) {
                platform_measure_text(child->node->content, child->node->style, available_width, &w, &h, &baseline);
            }
            
            child_w = w; child_h = h;
            child->fragment.border_box.width = w;
            child->fragment.border_box.height = h;
            child->fragment.baseline = baseline;
            child_ascent = baseline;
            child_descent = h - baseline;
        } else {
            layout_compute(child, space);
            child_w = child->fragment.border_box.width;
            child_h = child->fragment.border_box.height;
            child_ascent = child_h; // Treat non-text as having baseline at bottom
            child_descent = 0;

            if (line.width + child_w > available_width && line.count > 0) {
                flush_line(&line, x_start, y_cursor, available_width, box->node->style->text_align);
            }
        }

        if (line.count < MAX_LINE_FRAGMENTS) {
            line.items[line.count++] = child;
            line.width += child_w;
            if (child_ascent > line.max_ascent) line.max_ascent = child_ascent;
            if (child_descent > line.max_descent) line.max_descent = child_descent;
        }
        child = child->next_sibling;
    }
    flush_line(&line, x_start, y_cursor, available_width, box->node->style->text_align);
}

// --- Table Layout (Two-Pass) ---

static void layout_table(layout_box_t *box, constraint_space_t space) {
    // 1. Collect cells and calculate preferred widths
    int max_cells_in_row = 0;
    layout_box_t *row = box->first_child;
    while (row) {
        if (row->node->style->display == DISPLAY_TABLE_ROW) {
            int count = 0;
            layout_box_t *cell = row->first_child;
            while (cell) { if (cell->node->style->display == DISPLAY_TABLE_CELL) count++; cell = cell->next_sibling; }
            if (count > max_cells_in_row) max_cells_in_row = count;
        }
        row = row->next_sibling;
    }

    if (max_cells_in_row == 0) return;

    // 2. Resolve widths (Simple distribution)
    int cell_w = box->fragment.content_box.width / max_cells_in_row;

    // 3. Layout rows and cells
    int child_y = box->fragment.content_box.y;
    row = box->first_child;
    while (row) {
        if (row->node->style->display == DISPLAY_TABLE_ROW) {
            row->fragment.border_box.x = box->fragment.content_box.x;
            row->fragment.border_box.y = child_y;
            row->fragment.border_box.width = box->fragment.content_box.width;
            row->fragment.content_box = row->fragment.border_box;

            int x_offset = row->fragment.content_box.x;
            int max_row_h = 0;
            layout_box_t *cell = row->first_child;
            while (cell) {
                if (cell->node->style->display == DISPLAY_TABLE_CELL) {
                    constraint_space_t cell_space = {cell_w, 0, 1, 0};
                    cell->fragment.border_box.x = x_offset;
                    cell->fragment.border_box.y = child_y;
                    layout_compute(cell, cell_space);
                    if (cell->fragment.border_box.height > max_row_h) max_row_h = cell->fragment.border_box.height;
                    x_offset += cell_w;
                }
                cell = cell->next_sibling;
            }
            row->fragment.border_box.height = max_row_h;
            child_y += max_row_h;
        }
        row = row->next_sibling;
    }
    box->fragment.border_box.height = child_y - box->fragment.border_box.y + (box->node->style->padding_bottom + box->node->style->border_width);
}

// --- Main Layout ---

void layout_compute(layout_box_t *box, constraint_space_t space) {
    if (!box || !box->node || !box->node->style) return;
    style_t *style = box->node->style;
    if (style->display == DISPLAY_NONE) return;

    box->last_space = space;
    int bw = style->border_width;
    int pl = style->padding_left, pr = style->padding_right, pt = style->padding_top, pb = style->padding_bottom;
    
    // Resolve Width
    if (style->display == DISPLAY_BLOCK || style->display == DISPLAY_TABLE ||
        style->display == DISPLAY_TABLE_ROW || style->display == DISPLAY_TABLE_CELL) {
        if (style->width > 0) box->fragment.border_box.width = style->width + (bw * 2) + pl + pr;
        else box->fragment.border_box.width = space.available_width;
    } else if (box->node->tag_name && strcasecmp(box->node->tag_name, "img") == 0) {
        box->fragment.border_box.width = (style->width > 0) ? style->width : 100;
    }

    box->fragment.content_box.width = box->fragment.border_box.width - (bw * 2 + pl + pr);
    if (box->fragment.content_box.width < 0) box->fragment.content_box.width = 0;
    box->fragment.content_box.x = box->fragment.border_box.x + bw + pl;
    box->fragment.content_box.y = box->fragment.border_box.y + bw + pt;

    if (style->display == DISPLAY_TABLE) {
        layout_table(box, space);
        return;
    }

    int child_y = box->fragment.content_box.y;
    int has_block_children = 0;
    layout_box_t *c = box->first_child;
    while (c) { if (is_block(c->node)) { has_block_children = 1; break; } c = c->next_sibling; }

    if (has_block_children) {
        layout_box_t *child = box->first_child;
        int prev_margin_bottom = 0;
        while (child) {
            if (child->node->style->display == DISPLAY_NONE) { child = child->next_sibling; continue; }
            int collapse = 0;
            if (is_block(child->node)) {
                int cur_mt = child->node->style->margin_top;
                collapse = (cur_mt > prev_margin_bottom) ? cur_mt : prev_margin_bottom;
                if (child == box->first_child) collapse = cur_mt;
                child_y += collapse;
            }
            constraint_space_t child_space = {box->fragment.content_box.width, 0, 1, 0};
            child->fragment.border_box.x = box->fragment.content_box.x + child->node->style->margin_left;
            child->fragment.border_box.y = child_y;
            layout_compute(child, child_space);
            child_y += child->fragment.border_box.height;
            if (is_block(child->node)) prev_margin_bottom = child->node->style->margin_bottom;
            child = child->next_sibling;
        }
        child_y += prev_margin_bottom;
    } else {
        layout_inline_children(box, space, &child_y);
        
        // For inline containers (like <b>, <span>), the width is the width of the content.
        // layout_inline_children calculates positions, but we need to sum up the width or find the bounding box.
        if (style->display == DISPLAY_INLINE) {
            int max_x = 0;
            layout_box_t *k = box->first_child;
            while (k) {
                int right_edge = k->fragment.border_box.x + k->fragment.border_box.width;
                if (right_edge > max_x) max_x = right_edge;
                k = k->next_sibling;
            }
            // Relative to content box x (which is usually 0 for inline recursion start)
            int content_w = max_x - box->fragment.content_box.x; 
            if (content_w < 0) content_w = 0;
            
            box->fragment.border_box.width = content_w + (bw * 2) + pl + pr;
            box->fragment.content_box.width = content_w;
        }
    }

    int content_height = child_y - (box->fragment.border_box.y + bw + pt);
    if (style->height > 0) box->fragment.border_box.height = style->height + (bw * 2) + pt + pb;
    else box->fragment.border_box.height = content_height + (bw * 2) + pt + pb;
    if (box->node->type == DOM_NODE_TEXT && box->fragment.border_box.height == 0) box->fragment.border_box.height = 16;
}

layout_box_t* layout_create_tree(node_t *root, int container_width) {
    if (!root) return NULL;
    layout_box_t *box = layout_create_box(root);
    node_t *child_node = root->first_child;
    layout_box_t *last_child_box = NULL;
    while (child_node) {
        layout_box_t *child_box = layout_create_tree(child_node, container_width);
        if (!box->first_child) box->first_child = child_box;
        else last_child_box->next_sibling = child_box;
        last_child_box = child_box;
        child_node = child_node->next_sibling;
    }
    if (root->tag_name && strcmp(root->tag_name, "root") == 0) {
        constraint_space_t space = {container_width, 0, 1, 0};
        box->fragment.border_box.x = 0; box->fragment.border_box.y = 0;
        layout_compute(box, space);
    }
    return box;
}