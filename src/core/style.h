#ifndef STYLE_H
#define STYLE_H

#include <stdint.h>

typedef enum {
    DISPLAY_BLOCK,
    DISPLAY_INLINE,
    DISPLAY_INLINE_BLOCK,  // CSS2: Acts like inline but can have width/height
    DISPLAY_LIST_ITEM,     // CSS2: For <li>, acts like block with marker
    DISPLAY_NONE,
    DISPLAY_TABLE,
    DISPLAY_TABLE_ROW,
    DISPLAY_TABLE_CELL
} display_t;

typedef enum {
    TEXT_ALIGN_LEFT,
    TEXT_ALIGN_CENTER,
    TEXT_ALIGN_RIGHT
} text_align_t;

typedef enum {
    FONT_STYLE_NORMAL,
    FONT_STYLE_ITALIC
} font_style_t;

typedef enum {
    TEXT_DECORATION_NONE,
    TEXT_DECORATION_UNDERLINE,
    TEXT_DECORATION_LINE_THROUGH
} text_decoration_t;

typedef enum {
    FONT_FAMILY_SERIF,
    FONT_FAMILY_SANS_SERIF,
    FONT_FAMILY_MONOSPACE
} font_family_t;

typedef enum {
    POSITION_STATIC,
    POSITION_RELATIVE,
    POSITION_ABSOLUTE,
    POSITION_FIXED
} position_t;

// CSS2: Float property
typedef enum {
    FLOAT_NONE,
    FLOAT_LEFT,
    FLOAT_RIGHT
} float_t;

// CSS2: Clear property (for interacting with floats)
typedef enum {
    CLEAR_NONE,
    CLEAR_LEFT,
    CLEAR_RIGHT,
    CLEAR_BOTH
} clear_t;

// CSS2: Overflow property
typedef enum {
    OVERFLOW_VISIBLE,
    OVERFLOW_HIDDEN,
    OVERFLOW_SCROLL,
    OVERFLOW_AUTO
} overflow_t;

typedef struct {
    uint32_t color;
    uint32_t bg_color;
    char *bg_image; // URL
    int margin_top, margin_bottom, margin_left, margin_right;
    int padding_top, padding_bottom, padding_left, padding_right;
    int border_width;
    int width, height;

    // Positioning
    position_t position;
    int top, right, bottom, left;

    // CSS2 Normal Flow properties
    float_t float_prop;      // Float: left, right, none
    clear_t clear;           // Clear: left, right, both, none
    overflow_t overflow;     // Overflow: visible, hidden, scroll, auto

    // Font properties
    int font_size;   // In pixels (approx)
    int font_weight; // 400 = normal, 700 = bold
    font_style_t font_style;
    font_family_t font_family;
    text_decoration_t text_decoration;

    display_t display;
    text_align_t text_align;
} style_t;

struct node_s;

void style_init_default(style_t *style);
void style_compute(struct node_s *node);

#endif // STYLE_H
