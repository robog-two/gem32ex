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
        
        // Align to baseline
        if (child->node->type == DOM_NODE_TEXT) {
            child->fragment.border_box.y = baseline_y - child->fragment.baseline;
        } else {
            // For blocks/images in inline flow, bottom align to baseline by default
            child->fragment.border_box.y = baseline_y - child->fragment.border_box.height;
        }
        
        current_x += child->fragment.border_box.width;
    }

    *y_cursor += line_height;
    line->count = 0;
    line->width = 0;
    line->max_ascent = 0;
    line->max_descent = 0;
}

static void layout_inline_children(layout_box_t *box, constraint_space_t space, int *y_cursor) {
    line_info_t line = {0};
    // Relative X start (from parent's content box left edge)
    // Parent content box x is relative to parent border box.
    // So if we want child pos relative to parent border box, we start at content box offset.
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
            // Recursively layout child
            // Note: child position will be set by flush_line relative to *this* box.
            // But layout_compute needs child to have some constraints.
            // And layout_compute for INLINE children needs to return size.
            // The child's recursive layout will set its own children's relative positions (0-based inside child).
            layout_compute(child, space);
            child_w = child->fragment.border_box.width;
            child_h = child->fragment.border_box.height;
            child_ascent = child_h; 
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
    int child_y = box->fragment.content_box.y; // Relative to box
    row = box->first_child;
    while (row) {
        if (row->node->style->display == DISPLAY_TABLE_ROW) {
            row->fragment.border_box.x = box->fragment.content_box.x;
            row->fragment.border_box.y = child_y;
            row->fragment.border_box.width = box->fragment.content_box.width;
            row->fragment.content_box = row->fragment.border_box; // Rows have no padding/border usually

            // For children of row (cells), coordinates relative to ROW
            int x_offset = 0; // Relative to row
            int max_row_h = 0;
            layout_box_t *cell = row->first_child;
            while (cell) {
                if (cell->node->style->display == DISPLAY_TABLE_CELL) {
                    constraint_space_t cell_space = {cell_w, 0, 1, 0};
                    cell->fragment.border_box.x = x_offset;
                    cell->fragment.border_box.y = 0; // Top of row
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
    // child_y is the bottom edge of the last row, relative to the table's border box (0,0).
    // The total height is this bottom edge + padding_bottom + border_width.
    // Note: box->fragment.content_box.y already accounted for top border/padding.
    box->fragment.border_box.height = child_y + box->node->style->padding_bottom + box->node->style->border_width;
}

// --- Main Layout ---

void layout_compute(layout_box_t *box, constraint_space_t space) {
    if (!box || !box->node || !box->node->style) return;
    style_t *style = box->node->style;
    if (style->display == DISPLAY_NONE) return;

    box->last_space = space;
    int bw = style->border_width;
    int pl = style->padding_left, pr = style->padding_right, pt = style->padding_top, pb = style->padding_bottom;
    int ml = style->margin_left, mr = style->margin_right;

    // Resolve Width
    if (style->width > 0) {
        box->fragment.border_box.width = style->width + (bw * 2) + pl + pr;
    } else if (style->display == DISPLAY_BLOCK || style->display == DISPLAY_TABLE ||
               style->display == DISPLAY_TABLE_ROW || style->display == DISPLAY_TABLE_CELL) {
        // For block elements, border box width = available width minus horizontal margins
        // (margins are outside the border box)
        box->fragment.border_box.width = space.available_width - ml - mr;
        if (box->fragment.border_box.width < 0) box->fragment.border_box.width = 0;
    } else if (box->node->tag_name && strcasecmp(box->node->tag_name, "img") == 0) {
        box->fragment.border_box.width = 100; // Default image width
    } else {
        // Inline container default width 0, grows with content
        box->fragment.border_box.width = 0;
    }

    // Determine content box relative to border box (0,0)
    // border_box.x/y will be set by PARENT. Here we just set content offsets relative to self.
    // Wait, we need content_box absolute offsets? NO.
    // content_box.x/y should be offsets from border_box (0,0).
    // So content_box.x = bw + pl.
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
            int collapse = 0;
            if (is_block(child->node)) {
                int cur_mt = child->node->style->margin_top;
                // Collapse with previous sibling's bottom margin
                collapse = (cur_mt > prev_margin_bottom) ? cur_mt : prev_margin_bottom;
                // First child: only add margin if parent has padding/border (which prevents collapse)
                if (is_first_child) {
                    // If parent has top padding or border, don't collapse through
                    if (pt > 0 || bw > 0) {
                        collapse = cur_mt;
                    } else {
                        // Margin collapses through - don't add it to child_y
                        collapse = 0;
                    }
                }
                child_y += collapse;
                is_first_child = 0;
            }
            // Pass parent's content box width as available width to child
            // Child will subtract its own margins from this
            constraint_space_t child_space = {box->fragment.content_box.width, 0, 1, 0};

            // Set child position relative to THIS box
            // child->x = content_left + margin_left
            if (is_block(child->node)) {
                 child->fragment.border_box.x = box->fragment.content_box.x + child->node->style->margin_left;
            } else {
                 child->fragment.border_box.x = box->fragment.content_box.x; // Wrapped in anon block conceptually
            }
            child->fragment.border_box.y = child_y;

            layout_compute(child, child_space);
            child_y += child->fragment.border_box.height;
            if (is_block(child->node)) prev_margin_bottom = child->node->style->margin_bottom;
            child = child->next_sibling;
        }
        // Last child's bottom margin: only add if parent has bottom padding or border
        // Otherwise it collapses through the parent
        if (pb > 0 || bw > 0) {
            child_y += prev_margin_bottom;
        }
    } else {
        layout_inline_children(box, space, &child_y);
        
        // For inline containers (like <b>, <span>), grow width to fit content
        if (style->display == DISPLAY_INLINE && style->width <= 0) {
            int max_x = 0;
            layout_box_t *k = box->first_child;
            while (k) {
                int right_edge = k->fragment.border_box.x + k->fragment.border_box.width;
                if (right_edge > max_x) max_x = right_edge;
                k = k->next_sibling;
            }
            // Content width is max_x - content_start
            int content_w = max_x - box->fragment.content_box.x; 
            if (content_w < 0) content_w = 0;
            
            // Update dims
            box->fragment.content_box.width = content_w;
            box->fragment.border_box.width = content_w + (bw * 2) + pl + pr;
        }
    }

    // Height calculation
    int content_height = child_y - (box->fragment.content_box.y); // child_y is relative to border box top
    if (content_height < 0) content_height = 0;

    if (style->height > 0) box->fragment.border_box.height = style->height + (bw * 2) + pt + pb;
    else box->fragment.border_box.height = content_height + (bw * 2) + pt + pb;
    
    if (box->node->type == DOM_NODE_TEXT && box->fragment.border_box.height == 0) box->fragment.border_box.height = 16;
}

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

// Hit Testing
layout_box_t* layout_hit_test(layout_box_t *root, int x, int y) {
    if (!root) return NULL;
    
    // Check if point is inside root's border box
    // Note: root positions are typically relative to parent. 
    // This function assumes x, y are local to 'root's parent coordinate space'.
    // BUT for recursion, we want to transform x, y to be local to 'root'.
    
    if (x >= root->fragment.border_box.x && 
        x < root->fragment.border_box.x + root->fragment.border_box.width &&
        y >= root->fragment.border_box.y &&
        y < root->fragment.border_box.y + root->fragment.border_box.height) {
        
        // Point is inside this box. Check children.
        // Transform x, y to local coordinates
        int local_x = x - root->fragment.border_box.x;
        int local_y = y - root->fragment.border_box.y;
        
        // Iterate children (reverse order usually better for Z-index, but we have flat list)
        // We'll just iterate forward and return the *last* match (effectively top-most in simple flow).
        // Actually, creating a temporary list to iterate reverse is too much work. 
        // Forward iteration returns the "deepest" match which is usually what we want (e.g. text inside button).
        // Wait, if children overlap, later sibling is on top. So we should find the *last* matching child.
        
        layout_box_t *hit = NULL;
        layout_box_t *child = root->first_child;
        while (child) {
            layout_box_t *result = layout_hit_test(child, local_x, local_y);
            if (result) hit = result;
            child = child->next_sibling;
        }
        
        if (hit) return hit;
        return root; // No child hit, return self
    }
    
    return NULL;
}