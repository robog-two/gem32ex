#include "style.h"
#include "dom.h"
#include <string.h>
#include <strings.h>
#include <stdlib.h>

void style_init_default(style_t *style) {
    if (!style) return;
    memset(style, 0, sizeof(style_t));
    style->color = 0x000000; // Black
    style->bg_color = 0xFFFFFF; // White
    style->display = DISPLAY_INLINE; // Default is inline
}

void style_compute(node_t *node) {
    if (!node || !node->style) return;

    style_t *style = node->style;

    if (node->type == DOM_NODE_ELEMENT) {
        if (strcasecmp(node->tag_name, "root") == 0 ||
            strcasecmp(node->tag_name, "html") == 0) {
            style->display = DISPLAY_BLOCK;
        } else if (strcasecmp(node->tag_name, "script") == 0 ||
            strcasecmp(node->tag_name, "style") == 0 ||
            strcasecmp(node->tag_name, "head") == 0 ||
            strcasecmp(node->tag_name, "title") == 0 ||
            strcasecmp(node->tag_name, "meta") == 0 ||
            strcasecmp(node->tag_name, "link") == 0 ||
            strcasecmp(node->tag_name, "noscript") == 0) {
            style->display = DISPLAY_NONE;
        } else if (strcasecmp(node->tag_name, "body") == 0) {
            style->display = DISPLAY_BLOCK;
            style->margin_top = style->margin_bottom = 8;
            style->margin_left = style->margin_right = 8;
        } else if (strcasecmp(node->tag_name, "div") == 0 ||
                   strcasecmp(node->tag_name, "header") == 0 ||
                   strcasecmp(node->tag_name, "footer") == 0 ||
                   strcasecmp(node->tag_name, "nav") == 0 ||
                   strcasecmp(node->tag_name, "main") == 0 ||
                   strcasecmp(node->tag_name, "section") == 0 ||
                   strcasecmp(node->tag_name, "article") == 0 ||
                   strcasecmp(node->tag_name, "aside") == 0) {
            style->display = DISPLAY_BLOCK;
        } else if (strcasecmp(node->tag_name, "p") == 0) {
            style->display = DISPLAY_BLOCK;
            style->margin_top = style->margin_bottom = 16;
        } else if (strcasecmp(node->tag_name, "h1") == 0) {
            style->display = DISPLAY_BLOCK;
            style->margin_top = style->margin_bottom = 21; // 0.67em
            style->color = 0x000000;
        } else if (strcasecmp(node->tag_name, "h2") == 0) {
            style->display = DISPLAY_BLOCK;
            style->margin_top = style->margin_bottom = 20; // 0.83em
        } else if (strcasecmp(node->tag_name, "h3") == 0) {
            style->display = DISPLAY_BLOCK;
            style->margin_top = style->margin_bottom = 18; // 1em
        } else if (strcasecmp(node->tag_name, "ul") == 0 || 
                   strcasecmp(node->tag_name, "ol") == 0) {
            style->display = DISPLAY_BLOCK;
            style->margin_top = style->margin_bottom = 16;
            style->padding_left = 40;
        } else if (strcasecmp(node->tag_name, "li") == 0) {
            style->display = DISPLAY_BLOCK;
        } else if (strcasecmp(node->tag_name, "table") == 0) {
            style->display = DISPLAY_TABLE;
        } else if (strcasecmp(node->tag_name, "tr") == 0) {
            style->display = DISPLAY_TABLE_ROW;
        } else if (strcasecmp(node->tag_name, "td") == 0 ||
                   strcasecmp(node->tag_name, "th") == 0) {
            style->display = DISPLAY_TABLE_CELL;
            style->padding_top = style->padding_bottom = 1;
            style->padding_left = style->padding_right = 1;
        } else if (strcasecmp(node->tag_name, "img") == 0) {
            style->display = DISPLAY_INLINE;
        }
    }

    // Inherit text alignment if not set
    if (node->parent && node->parent->style) {
        if (style->text_align == TEXT_ALIGN_LEFT && node->parent->style->text_align != TEXT_ALIGN_LEFT) {
            // This is a simplification; in real CSS alignment inherits
            // but for now we only set it if explicitly defined or via attributes
        }
    }

    // Process attributes
    attr_t *attr = node->attributes;
    while (attr) {
        if (strcasecmp(attr->name, "align") == 0 && attr->value) {
            if (strcasecmp(attr->value, "center") == 0) style->text_align = TEXT_ALIGN_CENTER;
            else if (strcasecmp(attr->value, "right") == 0) style->text_align = TEXT_ALIGN_RIGHT;
            else if (strcasecmp(attr->value, "left") == 0) style->text_align = TEXT_ALIGN_LEFT;
        } else if (strcasecmp(attr->name, "width") == 0 && attr->value) {
            style->width = atoi(attr->value);
        } else if (strcasecmp(attr->name, "height") == 0 && attr->value) {
            style->height = atoi(attr->value);
        } else if (strcasecmp(attr->name, "border") == 0 && attr->value) {
            style->border_width = atoi(attr->value);
        }
        attr = attr->next;
    }

    if (node->tag_name && strcasecmp(node->tag_name, "center") == 0) {
        style->display = DISPLAY_BLOCK;
        style->text_align = TEXT_ALIGN_CENTER;
    }

    node_t *child = node->first_child;
    while (child) {
        style_compute(child);
        child = child->next_sibling;
    }
}
