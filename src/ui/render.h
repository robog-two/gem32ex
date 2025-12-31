#ifndef RENDER_H
#define RENDER_H

#include <windows.h>
#include "../core/layout.h"

extern node_t *g_focused_node;

void render_tree(HDC hdc, layout_box_t *box, int offset_x, int offset_y);

#endif // RENDER_H
