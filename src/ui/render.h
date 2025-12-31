#ifndef RENDER_H
#define RENDER_H

#include <windows.h>
#include "../core/layout.h"

extern node_t *g_focused_node;

void render_init(void);
void render_cleanup(void);
void render_tree(HDC hdc, layout_box_t *box, int offset_x, int offset_y);
void render_image_data(HDC hdc, void *data, size_t size, int x, int y, int w, int h);

#endif // RENDER_H