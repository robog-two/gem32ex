#include "css_property.h"
#include "css_name_colors.h"
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>

// Parse URL from url() notation
char* css_parse_url(const char *val) {
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

// Parse color value
uint32_t css_parse_color(const char *value) {
    if (!value) return 0xFFFFFF;

    // Named colors (uses css_name_colors.h)
    uint32_t color;
    if (css_parse_named_color(value, &color)) {
        return color;
    }

    // Hex colors
    if (value[0] == '#') {
        const char *hex = value + 1;
        int len = strlen(hex);

        if (len == 6) {
            // #RRGGBB
            return (uint32_t)strtol(hex, NULL, 16);
        } else if (len == 3) {
            // #RGB -> #RRGGBB
            char expanded[7];
            expanded[0] = hex[0]; expanded[1] = hex[0];
            expanded[2] = hex[1]; expanded[3] = hex[1];
            expanded[4] = hex[2]; expanded[5] = hex[2];
            expanded[6] = '\0';
            return (uint32_t)strtol(expanded, NULL, 16);
        }
    }

    // TODO: rgb() and rgba() parsing
    // For now, return default
    return 0xFFFFFF;
}

// Parse dimension (handles px, em, %, etc.)
int css_parse_dimension(const char *value) {
    if (!value) return 0;

    // Parse number
    char *endptr;
    int num = (int)strtol(value, &endptr, 10);

    // TODO: Handle units properly (em, rem, %, etc.)
    // For now, treat everything as pixels

    return num;
}

// Trim whitespace from string (modifies in place)
static char* trim(char *str) {
    if (!str) return NULL;

    // Trim leading
    while (*str && isspace(*str)) str++;

    if (*str == '\0') return str;

    // Trim trailing
    char *end = str + strlen(str) - 1;
    while (end > str && isspace(*end)) *end-- = '\0';

    return str;
}

// Parse a single CSS property
int css_property_parse(style_t *style, const char *name, const char *value) {
    if (!style || !name || !value) return 0;

    // Make copies for trimming
    char name_buf[128], value_buf[256];
    strncpy(name_buf, name, sizeof(name_buf) - 1);
    name_buf[sizeof(name_buf) - 1] = '\0';
    strncpy(value_buf, value, sizeof(value_buf) - 1);
    value_buf[sizeof(value_buf) - 1] = '\0';

    char *n = trim(name_buf);
    char *v = trim(value_buf);

    // Ignore vendor prefixes and CSS variables
    if (n[0] == '-') return 0;

    // Display property
    if (strcasecmp(n, "display") == 0) {
        if (strcasecmp(v, "block") == 0) style->display = DISPLAY_BLOCK;
        else if (strcasecmp(v, "inline") == 0) style->display = DISPLAY_INLINE;
        else if (strcasecmp(v, "inline-block") == 0) style->display = DISPLAY_INLINE_BLOCK;
        else if (strcasecmp(v, "list-item") == 0) style->display = DISPLAY_LIST_ITEM;
        else if (strcasecmp(v, "none") == 0) style->display = DISPLAY_NONE;
        else if (strcasecmp(v, "table") == 0) style->display = DISPLAY_TABLE;
        else if (strcasecmp(v, "table-row") == 0) style->display = DISPLAY_TABLE_ROW;
        else if (strcasecmp(v, "table-cell") == 0) style->display = DISPLAY_TABLE_CELL;
        return 1;
    }

    // Position property
    if (strcasecmp(n, "position") == 0) {
        if (strcasecmp(v, "static") == 0) style->position = POSITION_STATIC;
        else if (strcasecmp(v, "relative") == 0) style->position = POSITION_RELATIVE;
        else if (strcasecmp(v, "absolute") == 0) style->position = POSITION_ABSOLUTE;
        else if (strcasecmp(v, "fixed") == 0) style->position = POSITION_FIXED;
        return 1;
    }

    // Float property
    if (strcasecmp(n, "float") == 0) {
        if (strcasecmp(v, "left") == 0) style->float_prop = FLOAT_LEFT;
        else if (strcasecmp(v, "right") == 0) style->float_prop = FLOAT_RIGHT;
        else if (strcasecmp(v, "none") == 0) style->float_prop = FLOAT_NONE;
        return 1;
    }

    // Clear property
    if (strcasecmp(n, "clear") == 0) {
        if (strcasecmp(v, "left") == 0) style->clear = CLEAR_LEFT;
        else if (strcasecmp(v, "right") == 0) style->clear = CLEAR_RIGHT;
        else if (strcasecmp(v, "both") == 0) style->clear = CLEAR_BOTH;
        else if (strcasecmp(v, "none") == 0) style->clear = CLEAR_NONE;
        return 1;
    }

    // Overflow property
    if (strcasecmp(n, "overflow") == 0) {
        if (strcasecmp(v, "visible") == 0) style->overflow = OVERFLOW_VISIBLE;
        else if (strcasecmp(v, "hidden") == 0) style->overflow = OVERFLOW_HIDDEN;
        else if (strcasecmp(v, "scroll") == 0) style->overflow = OVERFLOW_SCROLL;
        else if (strcasecmp(v, "auto") == 0) style->overflow = OVERFLOW_AUTO;
        return 1;
    }

    // Dimensions
    if (strcasecmp(n, "width") == 0) {
        style->width = css_parse_dimension(v);
        return 1;
    }
    if (strcasecmp(n, "height") == 0) {
        style->height = css_parse_dimension(v);
        return 1;
    }

    // Positioning offsets
    if (strcasecmp(n, "top") == 0) {
        style->top = css_parse_dimension(v);
        return 1;
    }
    if (strcasecmp(n, "left") == 0) {
        style->left = css_parse_dimension(v);
        return 1;
    }
    if (strcasecmp(n, "right") == 0) {
        style->right = css_parse_dimension(v);
        return 1;
    }
    if (strcasecmp(n, "bottom") == 0) {
        style->bottom = css_parse_dimension(v);
        return 1;
    }

    // Margins
    if (strcasecmp(n, "margin-top") == 0) {
        style->margin_top = css_parse_dimension(v);
        return 1;
    }
    if (strcasecmp(n, "margin-bottom") == 0) {
        style->margin_bottom = css_parse_dimension(v);
        return 1;
    }
    if (strcasecmp(n, "margin-left") == 0) {
        style->margin_left = css_parse_dimension(v);
        return 1;
    }
    if (strcasecmp(n, "margin-right") == 0) {
        style->margin_right = css_parse_dimension(v);
        return 1;
    }

    // Padding
    if (strcasecmp(n, "padding-top") == 0) {
        style->padding_top = css_parse_dimension(v);
        return 1;
    }
    if (strcasecmp(n, "padding-bottom") == 0) {
        style->padding_bottom = css_parse_dimension(v);
        return 1;
    }
    if (strcasecmp(n, "padding-left") == 0) {
        style->padding_left = css_parse_dimension(v);
        return 1;
    }
    if (strcasecmp(n, "padding-right") == 0) {
        style->padding_right = css_parse_dimension(v);
        return 1;
    }

    // Border
    if (strcasecmp(n, "border-width") == 0) {
        style->border_width = css_parse_dimension(v);
        return 1;
    }

    // Colors
    if (strcasecmp(n, "color") == 0) {
        style->color = css_parse_color(v);
        return 1;
    }
    if (strcasecmp(n, "background-color") == 0) {
        style->bg_color = css_parse_color(v);
        return 1;
    }

    // Background image
    if (strcasecmp(n, "background-image") == 0) {
        if (style->bg_image) free(style->bg_image);
        style->bg_image = css_parse_url(v);
        return 1;
    }

    // Font properties
    if (strcasecmp(n, "font-size") == 0) {
        style->font_size = css_parse_dimension(v);
        return 1;
    }
    if (strcasecmp(n, "font-weight") == 0) {
        if (strcasecmp(v, "normal") == 0) style->font_weight = 400;
        else if (strcasecmp(v, "bold") == 0) style->font_weight = 700;
        else style->font_weight = atoi(v);
        return 1;
    }
    if (strcasecmp(n, "font-style") == 0) {
        if (strcasecmp(v, "italic") == 0) style->font_style = FONT_STYLE_ITALIC;
        else style->font_style = FONT_STYLE_NORMAL;
        return 1;
    }
    if (strcasecmp(n, "font-family") == 0) {
        if (strstr(v, "monospace")) style->font_family = FONT_FAMILY_MONOSPACE;
        else if (strstr(v, "sans-serif")) style->font_family = FONT_FAMILY_SANS_SERIF;
        else style->font_family = FONT_FAMILY_SERIF;
        return 1;
    }

    // Text properties
    if (strcasecmp(n, "text-align") == 0) {
        if (strcasecmp(v, "center") == 0) style->text_align = TEXT_ALIGN_CENTER;
        else if (strcasecmp(v, "right") == 0) style->text_align = TEXT_ALIGN_RIGHT;
        else style->text_align = TEXT_ALIGN_LEFT;
        return 1;
    }
    if (strcasecmp(n, "text-decoration") == 0) {
        if (strcasecmp(v, "underline") == 0) style->text_decoration = TEXT_DECORATION_UNDERLINE;
        else if (strcasecmp(v, "line-through") == 0) style->text_decoration = TEXT_DECORATION_LINE_THROUGH;
        else style->text_decoration = TEXT_DECORATION_NONE;
        return 1;
    }

    return 0;  // Unknown property
}

// Parse a CSS declaration block
void css_properties_parse_block(style_t *style, const char *css_block) {
    if (!style || !css_block) return;

    char *copy = strdup(css_block);
    if (!copy) return;

    char *p = copy;
    while (*p) {
        // Find next semicolon
        char *end = strchr(p, ';');
        if (end) *end = '\0';

        // Split on colon
        char *colon = strchr(p, ':');
        if (colon) {
            *colon = '\0';
            css_property_parse(style, p, colon + 1);
        }

        if (!end) break;
        p = end + 1;
    }

    free(copy);
}
