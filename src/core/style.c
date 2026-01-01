#include "style.h"
#include "dom.h"
#include "log.h"
#include "css_property.h"
#include "css_stylesheet.h"
#include <string.h>
#include <strings.h>
#include <stdlib.h>

/*
 * Style Computation Module
 *
 * This module coordinates the CSS cascade and style computation for DOM nodes.
 * It uses the modular CSS system:
 * - css_property.c: Property parsing
 * - css_selector.c: Selector matching
 * - css_stylesheet.c: Stylesheet management
 *
 * Style computation follows the CSS cascade:
 * 1. Initialize with defaults
 * 2. Inherit from parent
 * 3. Apply user-agent stylesheet
 * 4. Apply author stylesheets (external/internal)
 * 5. Apply inline styles
 * 6. Compute final values
 */

void style_init_default(style_t *style) {
    if (!style) return;
    memset(style, 0, sizeof(style_t));
    style->color = 0x000000; // Black
    style->bg_color = 0xFFFFFF; // White
    style->display = DISPLAY_INLINE; // Default is inline
    style->position = POSITION_STATIC; // Default positioning
    style->float_prop = FLOAT_NONE; // No float by default
    style->clear = CLEAR_NONE; // No clear by default
    style->overflow = OVERFLOW_VISIBLE; // Default overflow behavior
    style->font_size = 16;   // Default 16px
    style->font_weight = 400; // Normal
    style->font_style = FONT_STYLE_NORMAL;
    style->font_family = FONT_FAMILY_SERIF; // Default serif usually
    style->text_decoration = TEXT_DECORATION_NONE;
}

// All CSS property parsing is now in css_property.c
// This file now focuses on coordinating the cascade

/*
 * Main style computation function
 * Implements the CSS cascade for a DOM node
 */
void style_compute(node_t *node) {
    if (!node || !node->style) return;

    if (node->tag_name && strcmp(node->tag_name, "root") == 0) {
        LOG_INFO("Computing styles using modular CSS system...");
    }

    style_t *style = node->style;

    // Step 1: Inherit inheritable properties from parent
    if (node->parent && node->parent->style) {
        style->font_size = node->parent->style->font_size;
        style->font_weight = node->parent->style->font_weight;
        style->font_style = node->parent->style->font_style;
        style->font_family = node->parent->style->font_family;
        style->color = node->parent->style->color;
        style->text_align = node->parent->style->text_align;
        style->text_decoration = node->parent->style->text_decoration;
    }

    // Step 2: Apply user-agent stylesheet (browser defaults)
    css_stylesheet_t *ua_sheet = css_get_user_agent_stylesheet();
    if (ua_sheet) {
        css_stylesheet_apply_to_style(ua_sheet, node, style);
    }

    // Step 3: Apply author stylesheets would go here (external CSS)
    // TODO: Add support for <link> and <style> tags

    // Step 4: Apply inline styles (highest priority except !important)
    const char *inline_style = node_get_attr(node, "style");
    if (inline_style) {
        css_properties_parse_block(style, inline_style);
    }

    // Step 5: Browser-specific form element default dimensions/appearance
    // (These aren't part of CSS spec, they're browser UI defaults)
    if (node->type == DOM_NODE_ELEMENT && node->tag_name) {
        if (strcasecmp(node->tag_name, "html") == 0 ||
            strcasecmp(node->tag_name, "root") == 0) {
            // Ensure root is always block (even if user-agent sheet fails)
            if (style->display != DISPLAY_BLOCK) style->display = DISPLAY_BLOCK;
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
            style->display = DISPLAY_LIST_ITEM; // CSS2: list-item generates marker box
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
            css_properties_parse_block(style, attr->value);
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
            style->color = css_parse_color(attr->value);
        } else if (strcasecmp(attr->name, "bgcolor") == 0 && attr->value) {
            style->bg_color = css_parse_color(attr->value);
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
