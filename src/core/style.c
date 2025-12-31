#include "style.h"
#include "dom.h"
#include "log.h"
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
    style->font_style = FONT_STYLE_NORMAL;
    style->font_family = FONT_FAMILY_SERIF; // Default serif usually
    style->text_decoration = TEXT_DECORATION_NONE;
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
    if (strcasecmp(val, "navy") == 0) return 0x000080;
    if (strcasecmp(val, "maroon") == 0) return 0x800000;
    if (strcasecmp(val, "purple") == 0) return 0x800080;
    return 0;
}

static char* parse_url_value(const char *val) {
    if (!val) return NULL;
    if (strncasecmp(val, "url(", 4) == 0) {
        const char *start = val + 4;
        while (*start == ' ' || *start == '"' || *start == '\'') start++;
        const char *end = val + strlen(val) - 1;
        while (end > start && (*end == ' ' || *end == ')' || *end == '"' || *end == '\'')) end--;
        int len = (int)(end - start + 1);
        if (len <= 0) return NULL;
        char *ret = malloc(len + 1);
        memcpy(ret, start, len);
        ret[len] = '\0';
        return ret;
    }
    return strdup(val);
}

static void parse_css_property(style_t *style, char *name, char *val) {
    // Simple trim
    while (*name == ' ' || *name == '\t' || *name == '\n') name++;
    char *end = name + strlen(name) - 1;
    while (end > name && (*end == ' ' || *end == '\t' || *end == '\n')) *end-- = '\0';
    
    while (*val == ' ' || *val == '\t' || *val == '\n') val++;
    end = val + strlen(val) - 1;
    while (end > val && (*end == ' ' || *end == '\t' || *end == '\n')) *end-- = '\0';

    if (name[0] == '-') return; // Ignore variables and vendor prefixes

    if (strcasecmp(name, "position") == 0) {
        if (strcasecmp(val, "absolute") == 0) style->position = POSITION_ABSOLUTE;
        else if (strcasecmp(val, "relative") == 0) style->position = POSITION_RELATIVE;
        else if (strcasecmp(val, "fixed") == 0) style->position = POSITION_FIXED;
        else style->position = POSITION_STATIC;
    } else if (strcasecmp(name, "background-image") == 0) {
        if (style->bg_image) free(style->bg_image);
        style->bg_image = parse_url_value(val);
    } else if (strcasecmp(name, "top") == 0) style->top = atoi(val);
    else if (strcasecmp(name, "left") == 0) style->left = atoi(val);
    else if (strcasecmp(name, "right") == 0) style->right = atoi(val);
    else if (strcasecmp(name, "bottom") == 0) style->bottom = atoi(val);
    else if (strcasecmp(name, "width") == 0) style->width = atoi(val);
    else if (strcasecmp(name, "height") == 0) style->height = atoi(val);
    else if (strcasecmp(name, "display") == 0) {
        if (strcasecmp(val, "block") == 0) style->display = DISPLAY_BLOCK;
        else if (strcasecmp(val, "inline") == 0) style->display = DISPLAY_INLINE;
        else if (strcasecmp(val, "none") == 0) style->display = DISPLAY_NONE;
    }
}

static void parse_inline_style(style_t *style, const char *css) {
    if (!css) return;
    char *copy = strdup(css);
    if (!copy) return;
    
    char *p = copy;
    while (*p) {
        char *end = strchr(p, ';');
        if (end) *end = '\0';
        
        char *colon = strchr(p, ':');
        if (colon) {
            *colon = '\0';
            parse_css_property(style, p, colon + 1);
        }
        
        if (!end) break;
        p = end + 1;
    }
    free(copy);
}

void style_compute(node_t *node) {
    if (!node || !node->style) return;

    if (node->tag_name && strcmp(node->tag_name, "root") == 0) {
        LOG_INFO("Computing styles...");
    }

    // Inherit from parent
    if (node->parent && node->parent->style) {
        node->style->font_size = node->parent->style->font_size;
        node->style->font_weight = node->parent->style->font_weight;
        node->style->font_style = node->parent->style->font_style;
        node->style->font_family = node->parent->style->font_family;
        node->style->color = node->parent->style->color;
        node->style->text_align = node->parent->style->text_align;
        // Text decoration is usually not inherited in the CSS sense (it paints over children), 
        // but for a simple engine, inheriting 'state' might be easier, or we render it on the parent.
        // Let's inherit it for simplicity in rendering leaf nodes.
        node->style->text_decoration = node->parent->style->text_decoration;
    }

    style_t *style = node->style;

    if (node->type == DOM_NODE_ELEMENT && node->tag_name) {
        // Block Elements
        if (strcasecmp(node->tag_name, "html") == 0 || 
            strcasecmp(node->tag_name, "root") == 0) {
            style->display = DISPLAY_BLOCK;
        } else if (strcasecmp(node->tag_name, "body") == 0) {
            style->display = DISPLAY_BLOCK;
            style->margin_top = style->margin_bottom = 8;
            style->margin_left = style->margin_right = 8;
        } else if (strcasecmp(node->tag_name, "address") == 0 ||
                   strcasecmp(node->tag_name, "article") == 0 ||
                   strcasecmp(node->tag_name, "aside") == 0 ||
                   strcasecmp(node->tag_name, "blockquote") == 0 ||
                   strcasecmp(node->tag_name, "div") == 0 ||
                   strcasecmp(node->tag_name, "dl") == 0 ||
                   strcasecmp(node->tag_name, "figure") == 0 ||
                   strcasecmp(node->tag_name, "figcaption") == 0 ||
                   strcasecmp(node->tag_name, "footer") == 0 ||
                   strcasecmp(node->tag_name, "form") == 0 ||
                   strcasecmp(node->tag_name, "header") == 0 ||
                   strcasecmp(node->tag_name, "hr") == 0 ||
                   strcasecmp(node->tag_name, "main") == 0 ||
                   strcasecmp(node->tag_name, "nav") == 0 ||
                   strcasecmp(node->tag_name, "p") == 0 ||
                   strcasecmp(node->tag_name, "pre") == 0 ||
                   strcasecmp(node->tag_name, "section") == 0) {
            style->display = DISPLAY_BLOCK;
        } 
        
        // Specific Block Styles
        if (strcasecmp(node->tag_name, "p") == 0) {
            style->margin_top = style->margin_bottom = 16;
        } else if (strcasecmp(node->tag_name, "blockquote") == 0) {
            style->margin_top = style->margin_bottom = 16;
            style->margin_left = style->margin_right = 40;
        } else if (strcasecmp(node->tag_name, "center") == 0) {
            style->display = DISPLAY_BLOCK;
            style->text_align = TEXT_ALIGN_CENTER;
        } else if (strcasecmp(node->tag_name, "address") == 0) {
            style->font_style = FONT_STYLE_ITALIC;
        } else if (strcasecmp(node->tag_name, "pre") == 0) {
            style->font_family = FONT_FAMILY_MONOSPACE;
        } else if (strcasecmp(node->tag_name, "hr") == 0) {
            style->border_width = 1;
            style->margin_top = style->margin_bottom = 8;
        }

        // Headings
        else if (strcasecmp(node->tag_name, "h1") == 0) {
            style->display = DISPLAY_BLOCK;
            style->font_size = 32; // 2em
            style->font_weight = 700;
            style->margin_top = style->margin_bottom = 21; // 0.67em
        } else if (strcasecmp(node->tag_name, "h2") == 0) {
            style->display = DISPLAY_BLOCK;
            style->font_size = 24; // 1.5em
            style->font_weight = 700;
            style->margin_top = style->margin_bottom = 20; // 0.83em
        } else if (strcasecmp(node->tag_name, "h3") == 0) {
            style->display = DISPLAY_BLOCK;
            style->font_size = 19; // 1.17em (Firefox uses 18.72 but we round to 19)
            style->font_weight = 700;
            style->margin_top = style->margin_bottom = 19; // Match h3 font-size for 1em margin
        } else if (strcasecmp(node->tag_name, "h4") == 0) {
            style->display = DISPLAY_BLOCK;
            style->font_size = 16; // 1em
            style->font_weight = 700;
            style->margin_top = style->margin_bottom = 21; // 1.33em
        } else if (strcasecmp(node->tag_name, "h5") == 0) {
            style->display = DISPLAY_BLOCK;
            style->font_size = 13; // 0.83em
            style->font_weight = 700;
            style->margin_top = style->margin_bottom = 22; // 1.67em
        } else if (strcasecmp(node->tag_name, "h6") == 0) {
            style->display = DISPLAY_BLOCK;
            style->font_size = 11; // 0.67em
            style->font_weight = 700;
            style->margin_top = style->margin_bottom = 25; // 2.33em
        }

        // Lists
        else if (strcasecmp(node->tag_name, "ul") == 0 || 
                   strcasecmp(node->tag_name, "ol") == 0 || 
                   strcasecmp(node->tag_name, "menu") == 0 || 
                   strcasecmp(node->tag_name, "dir") == 0) {
            style->display = DISPLAY_BLOCK;
            style->margin_top = style->margin_bottom = 16;
            style->padding_left = 40;
        } else if (strcasecmp(node->tag_name, "li") == 0) {
            style->display = DISPLAY_BLOCK;
        } else if (strcasecmp(node->tag_name, "dl") == 0) {
            style->display = DISPLAY_BLOCK;
            style->margin_top = style->margin_bottom = 16;
        } else if (strcasecmp(node->tag_name, "dt") == 0) {
            style->display = DISPLAY_BLOCK;
            style->font_weight = 700;
        } else if (strcasecmp(node->tag_name, "dd") == 0) {
            style->display = DISPLAY_BLOCK;
            style->margin_left = 40;
        }

        // Tables
        else if (strcasecmp(node->tag_name, "table") == 0) {
            style->display = DISPLAY_TABLE;
            // style->border_collapse ... 
        } else if (strcasecmp(node->tag_name, "tr") == 0) {
            style->display = DISPLAY_TABLE_ROW;
        } else if (strcasecmp(node->tag_name, "td") == 0) {
            style->display = DISPLAY_TABLE_CELL;
            style->padding_left = style->padding_right = 1;
        } else if (strcasecmp(node->tag_name, "th") == 0) {
            style->display = DISPLAY_TABLE_CELL;
            style->font_weight = 700;
            style->text_align = TEXT_ALIGN_CENTER;
            style->padding_left = style->padding_right = 1;
        } else if (strcasecmp(node->tag_name, "caption") == 0) {
            style->display = DISPLAY_BLOCK; // Simplified
            style->text_align = TEXT_ALIGN_CENTER;
        }

        // Form Elements
        else if (strcasecmp(node->tag_name, "input") == 0 ||
                   strcasecmp(node->tag_name, "select") == 0 ||
                   strcasecmp(node->tag_name, "textarea") == 0 ||
                   strcasecmp(node->tag_name, "button") == 0) {
            style->display = DISPLAY_INLINE; // Treating as inline for now
            style->border_width = 1;
            style->padding_top = style->padding_bottom = 2;
            style->padding_left = style->padding_right = 4;
            
            if (strcasecmp(node->tag_name, "input") == 0) {
                 const char *type = node_get_attr(node, "type");
                 if (type && (strcasecmp(type, "submit") == 0 || strcasecmp(type, "button") == 0)) {
                     style->bg_color = 0xE1E1E1;
                     style->text_align = TEXT_ALIGN_CENTER;
                     style->width = 80;
                     style->height = 24;
                 } else {
                     style->bg_color = 0xFFFFFF;
                     style->width = 150;
                     style->height = 20;
                 }
            } else if (strcasecmp(node->tag_name, "button") == 0) {
                 style->bg_color = 0xE1E1E1; 
                 style->text_align = TEXT_ALIGN_CENTER;
                 style->width = 80;
                 style->height = 24;
            } else if (strcasecmp(node->tag_name, "select") == 0) {
                 style->bg_color = 0xFFFFFF;
                 style->width = 120;
                 style->height = 22;
            } else if (strcasecmp(node->tag_name, "textarea") == 0) {
                 style->bg_color = 0xFFFFFF;
                 style->width = 300;
                 style->height = 100;
            } else if (strcasecmp(node->tag_name, "iframe") == 0) {
                 style->display = DISPLAY_INLINE;
                 style->width = 300;
                 style->height = 150;
                 style->border_width = 1;
            }
        }

        // Inline Elements with special styles
        else if (strcasecmp(node->tag_name, "a") == 0) {
            style->display = DISPLAY_INLINE;
            style->color = 0x0000FF; // Blue
            style->text_decoration = TEXT_DECORATION_UNDERLINE;
        } else if (strcasecmp(node->tag_name, "font") == 0 ||
                   strcasecmp(node->tag_name, "span") == 0) {
            style->display = DISPLAY_INLINE;
        } else if (strcasecmp(node->tag_name, "b") == 0 ||
                   strcasecmp(node->tag_name, "strong") == 0) {
            style->display = DISPLAY_INLINE;
            style->font_weight = 700;
        } else if (strcasecmp(node->tag_name, "i") == 0 ||
                   strcasecmp(node->tag_name, "em") == 0 ||
                   strcasecmp(node->tag_name, "cite") == 0 ||
                   strcasecmp(node->tag_name, "var") == 0 ||
                   strcasecmp(node->tag_name, "dfn") == 0) {
            style->display = DISPLAY_INLINE;
            style->font_style = FONT_STYLE_ITALIC;
        } else if (strcasecmp(node->tag_name, "code") == 0 ||
                   strcasecmp(node->tag_name, "kbd") == 0 ||
                   strcasecmp(node->tag_name, "samp") == 0 ||
                   strcasecmp(node->tag_name, "tt") == 0) {
            style->display = DISPLAY_INLINE;
            style->font_family = FONT_FAMILY_MONOSPACE;
        } else if (strcasecmp(node->tag_name, "u") == 0 ||
                   strcasecmp(node->tag_name, "ins") == 0) {
            style->display = DISPLAY_INLINE;
            style->text_decoration = TEXT_DECORATION_UNDERLINE;
        } else if (strcasecmp(node->tag_name, "s") == 0 ||
                   strcasecmp(node->tag_name, "strike") == 0 ||
                   strcasecmp(node->tag_name, "del") == 0) {
            style->display = DISPLAY_INLINE;
            style->text_decoration = TEXT_DECORATION_LINE_THROUGH;
        } else if (strcasecmp(node->tag_name, "small") == 0) {
            style->display = DISPLAY_INLINE;
            style->font_size = (style->font_size * 8) / 10; // 0.8em approx
        } else if (strcasecmp(node->tag_name, "big") == 0) {
            style->display = DISPLAY_INLINE;
            style->font_size = (style->font_size * 12) / 10; // 1.2em approx
        } else if (strcasecmp(node->tag_name, "sub") == 0 || 
                   strcasecmp(node->tag_name, "sup") == 0) {
            style->display = DISPLAY_INLINE;
            style->font_size = (style->font_size * 8) / 10;
            // Vertical align not yet supported in layout
        } else if (strcasecmp(node->tag_name, "img") == 0) {
            style->display = DISPLAY_INLINE;
        } else if (strcasecmp(node->tag_name, "script") == 0 ||
                   strcasecmp(node->tag_name, "noscript") == 0 ||
                   strcasecmp(node->tag_name, "style") == 0 ||
                   strcasecmp(node->tag_name, "link") == 0 ||
                   strcasecmp(node->tag_name, "meta") == 0 ||
                   strcasecmp(node->tag_name, "head") == 0 ||
                   strcasecmp(node->tag_name, "title") == 0) {
            style->display = DISPLAY_NONE;
        }
    }

    // Process attributes
    attr_t *attr = node->attributes;
    while (attr) {
        if (strcasecmp(attr->name, "style") == 0 && attr->value) {
            parse_inline_style(style, attr->value);
        } else if (strcasecmp(attr->name, "align") == 0 && attr->value) {
            if (strcasecmp(attr->value, "center") == 0) style->text_align = TEXT_ALIGN_CENTER;
            else if (strcasecmp(attr->value, "right") == 0) style->text_align = TEXT_ALIGN_RIGHT;
            else if (strcasecmp(attr->value, "left") == 0) style->text_align = TEXT_ALIGN_LEFT;
        } else if (strcasecmp(attr->name, "width") == 0 && attr->value) {
            style->width = atoi(attr->value);
        } else if (strcasecmp(attr->name, "size") == 0 && attr->value) {
            if (node->tag_name && strcasecmp(node->tag_name, "input") == 0) {
                style->width = atoi(attr->value) * 8 + 10; // Approx 8px per char + some padding
            } else if (node->tag_name && strcasecmp(node->tag_name, "font") == 0) {
                // HTML font size attribute: 1-7 scale (traditional HTML)
                // Based on legacy browser behavior where 3=normal (16px)
                int size = atoi(attr->value);
                const int font_sizes[] = {10, 13, 16, 18, 24, 32, 48}; // 1 to 7
                if (size >= 1 && size <= 7) {
                    style->font_size = font_sizes[size - 1];
                }
            }
        } else if (strcasecmp(attr->name, "height") == 0 && attr->value) {
            style->height = atoi(attr->value);
        } else if (strcasecmp(attr->name, "border") == 0 && attr->value) {
            style->border_width = atoi(attr->value);
        } else if (strcasecmp(attr->name, "color") == 0 && attr->value) {
            style->color = parse_color(attr->value);
        } else if (strcasecmp(attr->name, "bgcolor") == 0 && attr->value) {
            style->bg_color = parse_color(attr->value);
        } else if (strcasecmp(attr->name, "background") == 0 && attr->value) {
            if (style->bg_image) free(style->bg_image);
            style->bg_image = strdup(attr->value);
        }
        attr = attr->next;
    }

    node_t *child = node->first_child;
    while (child) {
        style_compute(child);
        child = child->next_sibling;
    }
}