#ifndef STYLE_H
#define STYLE_H

#include <stdint.h>

typedef enum {
    DISPLAY_BLOCK,
    DISPLAY_INLINE,
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

typedef struct {
    uint32_t color;
    uint32_t bg_color;
    int margin_top, margin_bottom, margin_left, margin_right;
    int padding_top, padding_bottom, padding_left, padding_right;
    int border_width;
    int width, height;
    
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
