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
    style->font_size = 16;   // Default 16px
    style->font_weight = 400; // Normal
}

static uint32_t parse_color(const char *val) {
    if (!val) return 0;
    if (val[0] == '#') {
        return (uint32_t)strtol(val + 1, NULL, 16);
    }
    // Basic named colors
    if (strcasecmp(val, "red") == 0) return 0xFF0000;
    if (strcasecmp(val, "green") == 0) return 0x00FF00;
    if (strcasecmp(val, "blue") == 0) return 0x0000FF;
    if (strcasecmp(val, "white") == 0) return 0xFFFFFF;
    if (strcasecmp(val, "black") == 0) return 0x000000;
    if (strcasecmp(val, "gray") == 0) return 0x808080;
    return 0;
}

void style_compute(node_t *node) {
    if (!node || !node->style) return;

    // Inherit from parent
    if (node->parent && node->parent->style) {
        node->style->font_size = node->parent->style->font_size;
        node->style->font_weight = node->parent->style->font_weight;
        node->style->color = node->parent->style->color;
        node->style->text_align = node->parent->style->text_align;
    }

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
            style->margin_top = style->margin_bottom = 21; 
            style->color = 0x000000;
            style->font_size = 32;
            style->font_weight = 700;
        } else if (strcasecmp(node->tag_name, "h2") == 0) {
            style->display = DISPLAY_BLOCK;
            style->margin_top = style->margin_bottom = 20; 
            style->font_size = 24;
            style->font_weight = 700;
        } else if (strcasecmp(node->tag_name, "h3") == 0) {
            style->display = DISPLAY_BLOCK;
            style->margin_top = style->margin_bottom = 18; 
            style->font_size = 19;
            style->font_weight = 700;
        } else if (strcasecmp(node->tag_name, "b") == 0 ||
                   strcasecmp(node->tag_name, "strong") == 0) {
            style->display = DISPLAY_INLINE;
            style->font_weight = 700;
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
        } else if (strcasecmp(attr->name, "color") == 0 && attr->value) {
            style->color = parse_color(attr->value);
        } else if (strcasecmp(attr->name, "bgcolor") == 0 && attr->value) {
            style->bg_color = parse_color(attr->value);
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
