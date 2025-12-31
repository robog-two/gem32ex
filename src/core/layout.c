#include "layout.h"
#include "platform.h"
#include "log.h"
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
    if (box->iframe_root) layout_free(box->iframe_root);
    free(box);
}

static void get_cumulative_offset(layout_box_t *box, int *dx, int *dy);

static int is_block(node_t *node) {
    if (!node || !node->style) return 0;
    display_t d = node->style->display;
    return (d == DISPLAY_BLOCK || d == DISPLAY_TABLE || d == DISPLAY_TABLE_ROW || d == DISPLAY_TABLE_CELL);
}

static int is_inline_container(const char *tag) {
    if (!tag) return 0;
    return (strcasecmp(tag, "font") == 0 ||
            strcasecmp(tag, "b") == 0 ||
            strcasecmp(tag, "i") == 0 ||
            strcasecmp(tag, "strong") == 0 ||
            strcasecmp(tag, "em") == 0 ||
            strcasecmp(tag, "span") == 0 ||
            strcasecmp(tag, "a") == 0 ||
            strcasecmp(tag, "small") == 0);
}

#define MAX_LINE_FRAGMENTS 256
typedef struct {
    layout_box_t *items[MAX_LINE_FRAGMENTS];
    int count;
    int width;
    int height;
    int max_ascent;
    int max_descent;
} line_info_t;

static void flush_line(line_info_t *line, int x_start, int *y_cursor, int available_width, text_align_t align) {
    if (line->count == 0) return;

    int free_space = available_width - line->width;
    int start_offset = 0;
    if (align == TEXT_ALIGN_CENTER) start_offset = free_space / 2;
    else if (align == TEXT_ALIGN_RIGHT) start_offset = free_space;

    if (start_offset < 0) start_offset = 0;

    int line_height = line->max_ascent + line->max_descent;
    int baseline_y = *y_cursor + line->max_ascent;

    int current_x = x_start + start_offset;

    for (int i = 0; i < line->count; i++) {
        layout_box_t *child = line->items[i];
        child->fragment.border_box.x = current_x;

        if (child->node->type == DOM_NODE_TEXT) {
            child->fragment.border_box.y = baseline_y - child->fragment.baseline;
        } else {
            child->fragment.border_box.y = baseline_y - child->fragment.border_box.height;
        }

        current_x += child->fragment.border_box.width;

        if (child->node->style->position == POSITION_RELATIVE) {
            child->fragment.border_box.x += child->node->style->left;
            child->fragment.border_box.y += child->node->style->top;
        }
    }

    *y_cursor += line_height;
    line->count = 0;
    line->width = 0;
    line->max_ascent = 0;
    line->max_descent = 0;
}

static void layout_prepare_inline_item(layout_box_t *item, line_info_t *line, int x_start, int *y_cursor, int available_width, text_align_t align, constraint_space_t space) {
    if (!item || item->node->style->display == DISPLAY_NONE) return;

    if (item->node->style->position == POSITION_ABSOLUTE || item->node->style->position == POSITION_FIXED) {
        layout_compute(item, space);
        int dx = 0, dy = 0;
        get_cumulative_offset(item, &dx, &dy);
        item->fragment.border_box.x = item->node->style->left - dx;
        item->fragment.border_box.y = item->node->style->top - dy;
        return;
    }

    if (item->node->type == DOM_NODE_TEXT && item->node->content) {
        int w, h, baseline;
        platform_measure_text(item->node->content, item->node->style, -1, &w, &h, &baseline);

        if (line->width + w > available_width && line->count > 0) {
            flush_line(line, x_start, y_cursor, available_width, align);
        }

        if (w > available_width) {
            platform_measure_text(item->node->content, item->node->style, available_width, &w, &h, &baseline);
        }

        item->fragment.border_box.width = w;
        item->fragment.border_box.height = h;
        item->fragment.baseline = baseline;

        if (line->count < MAX_LINE_FRAGMENTS) {
            line->items[line->count++] = item;
            line->width += w;
            if (baseline > line->max_ascent) line->max_ascent = baseline;
            if ((h - baseline) > line->max_descent) line->max_descent = h - baseline;
        }
    } else if (item->node->style->display == DISPLAY_INLINE && is_inline_container(item->node->tag_name)) {
        layout_box_t *child = item->first_child;
        while (child) {
            layout_prepare_inline_item(child, line, x_start, y_cursor, available_width, align, space);
            child = child->next_sibling;
        }
    } else {
        // Atomic inline-block or other
        layout_compute(item, space);
        int child_w = item->fragment.border_box.width;
        int child_h = item->fragment.border_box.height;

        if (line->width + child_w > available_width && line->count > 0) {
            flush_line(line, x_start, y_cursor, available_width, align);
        }

        if (line->count < MAX_LINE_FRAGMENTS) {
            line->items[line->count++] = item;
            line->width += child_w;
            if (child_h > line->max_ascent) line->max_ascent = child_h;
        }
    }
}

/*
 * Layouts children in a horizontal flow (inline context).
 * Handles text measurement, line wrapping, and vertical alignment within lines.
 */
static void layout_inline_children(layout_box_t *box, constraint_space_t space, int *y_cursor) {
    line_info_t line = {0};
    
    int x_start = box->fragment.content_box.x;
    int available_width = box->fragment.content_box.width;

    if (box->node->style->display == DISPLAY_INLINE && box->node->style->width <= 0) {
        int decoration = box->node->style->padding_left + box->node->style->padding_right + 
                         (box->node->style->border_width * 2) + 
                         box->node->style->margin_left + box->node->style->margin_right;
        available_width = space.available_width - decoration;
        if (available_width < 0) available_width = 0;
    }

    text_align_t align = box->node->style->text_align;

    layout_box_t *child = box->first_child;
    while (child) {
        layout_prepare_inline_item(child, &line, x_start, y_cursor, available_width, align, space);
        child = child->next_sibling;
    }

    flush_line(&line, x_start, y_cursor, available_width, align);
}


static void layout_table(layout_box_t *box, constraint_space_t space) {
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

    int cell_w = box->fragment.content_box.width / max_cells_in_row;

    int child_y = box->fragment.content_box.y;
    row = box->first_child;
    while (row) {
        if (row->node->style->display == DISPLAY_TABLE_ROW) {
            row->fragment.border_box.x = box->fragment.content_box.x;
            row->fragment.border_box.y = child_y;
            row->fragment.border_box.width = box->fragment.content_box.width;
            row->fragment.content_box = row->fragment.border_box;

            int x_offset = 0;
            int max_row_h = 0;
            layout_box_t *cell = row->first_child;
            while (cell) {
                if (cell->node->style->display == DISPLAY_TABLE_CELL) {
                    constraint_space_t cell_space = {cell_w, 0, 1, 0};
                    cell->fragment.border_box.x = x_offset;
                    cell->fragment.border_box.y = 0;
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
    box->fragment.border_box.height = child_y + box->node->style->padding_bottom + box->node->style->border_width;
}


static void get_cumulative_offset(layout_box_t *box, int *dx, int *dy) {
    *dx = 0;
    *dy = 0;
    if (!box || !box->parent) return;
    
    // Start from parent, as box->x is relative to parent
    layout_box_t *p = box->parent;
    while (p) {
        if (p->node->style->position != POSITION_STATIC || !p->parent) {
             // Found the anchor (positioned element or root)
             break;
        }
        *dx += p->fragment.border_box.x;
        *dy += p->fragment.border_box.y;
        p = p->parent;
    }
}

void layout_compute(layout_box_t *box, constraint_space_t space) {
    if (!box || !box->node || !box->node->style) return;
    style_t *style = box->node->style;
    if (style->display == DISPLAY_NONE) return;

    box->last_space = space;
    int bw = style->border_width;
    int pl = style->padding_left, pr = style->padding_right, pt = style->padding_top, pb = style->padding_bottom;
    int ml = style->margin_left, mr = style->margin_right;
    
    if (style->width > 0) {
        box->fragment.border_box.width = style->width + (bw * 2) + pl + pr;
    } else if (style->display == DISPLAY_BLOCK || style->display == DISPLAY_TABLE ||
               style->display == DISPLAY_TABLE_ROW || style->display == DISPLAY_TABLE_CELL) {

        box->fragment.border_box.width = space.available_width - ml - mr;
        if (box->fragment.border_box.width < 0) box->fragment.border_box.width = 0;
    } else if (box->node->tag_name && strcasecmp(box->node->tag_name, "img") == 0) {
        box->fragment.border_box.width = 100;
    } else {
        if (style->display == DISPLAY_INLINE) {
             box->fragment.border_box.width = space.available_width - ml - mr;
             if (box->fragment.border_box.width < 0) box->fragment.border_box.width = 0;
        } else {
             box->fragment.border_box.width = 0;
        }
    }

    box->fragment.content_box.width = box->fragment.border_box.width - (bw * 2 + pl + pr);
    if (box->fragment.content_box.width < 0) box->fragment.content_box.width = 0;
    box->fragment.content_box.x = bw + pl;
    box->fragment.content_box.y = bw + pt;

    if (style->display == DISPLAY_TABLE) {
        layout_table(box, space);
        return;
    }

    int child_y = box->fragment.content_box.y; // Start at content top (relative)
    int has_block_children = 0;
    layout_box_t *c = box->first_child;
    while (c) { if (is_block(c->node)) { has_block_children = 1; break; } c = c->next_sibling; }

    if (has_block_children) {
        layout_box_t *child = box->first_child;
        int prev_margin_bottom = 0;
        int is_first_child = 1;
        while (child) {
            if (child->node->style->display == DISPLAY_NONE) { child = child->next_sibling; continue; }
            
            if (child->node->style->position == POSITION_ABSOLUTE || child->node->style->position == POSITION_FIXED) {
                constraint_space_t child_space = {box->fragment.content_box.width, 0, 1, 0};
                layout_compute(child, child_space);
                int dx = 0, dy = 0;
                get_cumulative_offset(child, &dx, &dy);
                child->fragment.border_box.x = child->node->style->left - dx;
                child->fragment.border_box.y = child->node->style->top - dy;
                child = child->next_sibling;
                continue;
            }

            int collapse = 0;
            if (is_block(child->node)) {
                int cur_mt = child->node->style->margin_top;

                /* 
                 * Margin Collapse Logic:
                 * Adjacent vertical margins of block-level boxes collapse.
                 * The resulting margin width is the maximum of the adjoining margin widths.
                 */
                collapse = (cur_mt > prev_margin_bottom) ? cur_mt : prev_margin_bottom;

                if (is_first_child) {
                    // Parent's padding/border prevents the first child's top margin from collapsing with parent's top margin (simplified view)
                    if (pt > 0 || bw > 0) {
                        collapse = cur_mt;
                    } else {
                        collapse = 0;
                    }
                } else {
                    collapse = (cur_mt > prev_margin_bottom) ? cur_mt : prev_margin_bottom;
                }
                child_y += collapse;
                is_first_child = 0;
            }

            constraint_space_t child_space = {box->fragment.content_box.width, 0, 1, 0};

            if (is_block(child->node)) {
                 child->fragment.border_box.x = box->fragment.content_box.x + child->node->style->margin_left;
            } else {
                 child->fragment.border_box.x = box->fragment.content_box.x; // Wrapped in anon block conceptually
            }
            child->fragment.border_box.y = child_y;

            layout_compute(child, child_space);
            
            if (child->node->style->position == POSITION_RELATIVE) {
                child->fragment.border_box.x += child->node->style->left;
                child->fragment.border_box.y += child->node->style->top;
            }

            child_y += child->fragment.border_box.height;
            if (is_block(child->node)) prev_margin_bottom = child->node->style->margin_bottom;
            child = child->next_sibling;
        }

        if (pb > 0 || bw > 0) {
            child_y += prev_margin_bottom;
        }
    } else {
        layout_inline_children(box, space, &child_y);

        if (style->display == DISPLAY_INLINE && style->width <= 0) {
            int max_x = 0;
            layout_box_t *k = box->first_child;
            while (k) {
                int right_edge = k->fragment.border_box.x + k->fragment.border_box.width;
                if (right_edge > max_x) max_x = right_edge;
                k = k->next_sibling;
            }
            int content_w = max_x - box->fragment.content_box.x;
            if (content_w < 0) content_w = 0;

            box->fragment.content_box.width = content_w;
            box->fragment.border_box.width = content_w + (bw * 2) + pl + pr;
        }
    }

    // Height calculation
    int content_height = child_y - (box->fragment.content_box.y); // child_y is relative to border box top
    if (content_height < 0) content_height = 0;

    if (style->height > 0) {
        box->fragment.border_box.height = style->height + (bw * 2) + pt + pb;
    } else {
        box->fragment.border_box.height = content_height + (bw * 2) + pt + pb;
    }

    if (box->node->type == DOM_NODE_TEXT && box->fragment.border_box.height == 0) box->fragment.border_box.height = 16;

    // Handle iFrames
    if (box->node->iframe_doc) {
        if (box->iframe_root) layout_free(box->iframe_root);
        box->iframe_root = layout_create_tree(box->node->iframe_doc, box->fragment.content_box.width);
    }
}

/*
 * Creates a parallel tree of layout_box_t structures from the DOM tree.
 * This separates the persistent DOM from the transient layout calculations.
 * After creation, it triggers the initial layout pass.
 */
layout_box_t* layout_create_tree(node_t *root, int container_width) {
    if (!root) return NULL;

    if (root->tag_name && strcmp(root->tag_name, "root") == 0) {
        LOG_INFO("Creating layout tree with width %d", container_width);
    }

    layout_box_t *box = layout_create_box(root);
    node_t *child_node = root->first_child;
    layout_box_t *last_child_box = NULL;
    while (child_node) {
        layout_box_t *child_box = layout_create_tree(child_node, container_width);
        child_box->parent = box;
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

layout_box_t* layout_hit_test(layout_box_t *root, int x, int y) {
    if (!root) return NULL;

    if (x >= root->fragment.border_box.x &&
        x < root->fragment.border_box.x + root->fragment.border_box.width &&
        y >= root->fragment.border_box.y &&
        y < root->fragment.border_box.y + root->fragment.border_box.height) {

        int local_x = x - root->fragment.border_box.x;
        int local_y = y - root->fragment.border_box.y;

        layout_box_t *hit = NULL;
        layout_box_t *child = root->first_child;
        while (child) {
            layout_box_t *result = layout_hit_test(child, local_x, local_y);
            if (result) hit = result;
            child = child->next_sibling;
        }

        if (hit) return hit;
        return root;
    }

    return NULL;
}
