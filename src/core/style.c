#include "style.h"
#include "dom.h"
#include <string.h>
#include <strings.h>

void style_init_default(style_t *style) {
    if (!style) return;
    memset(style, 0, sizeof(style_t));
    style->color = 0x000000; // Black
    style->bg_color = 0xFFFFFF; // White
    style->display = DISPLAY_INLINE; // Default is inline
}

void style_compute(node_t *node) {
    if (!node || !node->style) return;

    if (node->type == DOM_NODE_ELEMENT) {
        if (strcasecmp(node->tag_name, "div") == 0 ||
            strcasecmp(node->tag_name, "p") == 0 ||
            strcasecmp(node->tag_name, "h1") == 0 ||
            strcasecmp(node->tag_name, "h2") == 0 ||
            strcasecmp(node->tag_name, "h3") == 0 ||
            strcasecmp(node->tag_name, "ul") == 0 ||
            strcasecmp(node->tag_name, "li") == 0) {
            node->style->display = DISPLAY_BLOCK;
        } else if (strcasecmp(node->tag_name, "table") == 0) {
            node->style->display = DISPLAY_TABLE;
        } else if (strcasecmp(node->tag_name, "tr") == 0) {
            node->style->display = DISPLAY_TABLE_ROW;
        } else if (strcasecmp(node->tag_name, "td") == 0 ||
                   strcasecmp(node->tag_name, "th") == 0) {
            node->style->display = DISPLAY_TABLE_CELL;
        }
    }

    node_t *child = node->first_child;
    while (child) {
        style_compute(child);
        child = child->next_sibling;
    }
}
