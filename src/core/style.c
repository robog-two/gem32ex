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
        if (strcasecmp(node->tag_name, "script") == 0 ||
            strcasecmp(node->tag_name, "style") == 0 ||
            strcasecmp(node->tag_name, "head") == 0 ||
            strcasecmp(node->tag_name, "title") == 0 ||
            strcasecmp(node->tag_name, "meta") == 0 ||
            strcasecmp(node->tag_name, "link") == 0 ||
            strcasecmp(node->tag_name, "noscript") == 0) {
            node->style->display = DISPLAY_NONE;
        } else if (strcasecmp(node->tag_name, "body") == 0) {
            node->style->display = DISPLAY_BLOCK;
            node->style->margin_top = 8;
            node->style->margin_bottom = 8;
            node->style->margin_left = 8;
            node->style->margin_right = 8;
        } else if (strcasecmp(node->tag_name, "div") == 0 ||
                   strcasecmp(node->tag_name, "header") == 0 ||
                   strcasecmp(node->tag_name, "footer") == 0 ||
                   strcasecmp(node->tag_name, "nav") == 0 ||
                   strcasecmp(node->tag_name, "main") == 0 ||
                   strcasecmp(node->tag_name, "section") == 0 ||
                   strcasecmp(node->tag_name, "article") == 0 ||
                   strcasecmp(node->tag_name, "aside") == 0 ||
                   strcasecmp(node->tag_name, "address") == 0 ||
                   strcasecmp(node->tag_name, "blockquote") == 0 ||
                   strcasecmp(node->tag_name, "figure") == 0 ||
                   strcasecmp(node->tag_name, "form") == 0 ||
                   strcasecmp(node->tag_name, "hr") == 0) {
            node->style->display = DISPLAY_BLOCK;
        } else if (strcasecmp(node->tag_name, "p") == 0) {
            node->style->display = DISPLAY_BLOCK;
            node->style->margin_top = 16;
            node->style->margin_bottom = 16;
        } else if (strcasecmp(node->tag_name, "h1") == 0) {
            node->style->display = DISPLAY_BLOCK;
            node->style->margin_top = 22;
            node->style->margin_bottom = 22;
        } else if (strcasecmp(node->tag_name, "h2") == 0) {
            node->style->display = DISPLAY_BLOCK;
            node->style->margin_top = 20;
            node->style->margin_bottom = 20;
        } else if (strcasecmp(node->tag_name, "h3") == 0) {
            node->style->display = DISPLAY_BLOCK;
            node->style->margin_top = 18;
            node->style->margin_bottom = 18;
        } else if (strcasecmp(node->tag_name, "ul") == 0 || 
                   strcasecmp(node->tag_name, "ol") == 0 ||
                   strcasecmp(node->tag_name, "dl") == 0) {
            node->style->display = DISPLAY_BLOCK;
            node->style->margin_top = 16;
            node->style->margin_bottom = 16;
            node->style->padding_left = 40;
        } else if (strcasecmp(node->tag_name, "li") == 0 ||
                   strcasecmp(node->tag_name, "dt") == 0 ||
                   strcasecmp(node->tag_name, "dd") == 0) {
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

    // Check for align attribute or specific tags
    attr_t *attr = node->attributes;
    while (attr) {
        if (strcasecmp(attr->name, "align") == 0 && attr->value) {
            if (strcasecmp(attr->value, "center") == 0) node->style->text_align = TEXT_ALIGN_CENTER;
            else if (strcasecmp(attr->value, "right") == 0) node->style->text_align = TEXT_ALIGN_RIGHT;
            else node->style->text_align = TEXT_ALIGN_LEFT;
        }
        attr = attr->next;
    }

    if (node->tag_name && strcasecmp(node->tag_name, "center") == 0) {
        node->style->display = DISPLAY_BLOCK;
        node->style->text_align = TEXT_ALIGN_CENTER;
    }
}
