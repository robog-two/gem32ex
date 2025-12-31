#ifndef STYLE_H
#define STYLE_H

#include <stdint.h>

typedef enum {
    DISPLAY_BLOCK,
    DISPLAY_INLINE,
    DISPLAY_NONE
} display_t;

typedef struct {
    uint32_t color;
    uint32_t bg_color;
    int margin_top, margin_bottom, margin_left, margin_right;
    int padding_top, padding_bottom, padding_left, padding_right;
    int border_width;
    int width, height;
    display_t display;
} style_t;

void style_init_default(style_t *style);

#endif // STYLE_H
