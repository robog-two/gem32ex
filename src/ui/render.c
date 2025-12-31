#include "render.h"
#include <stdio.h>

static void set_color_from_style(HDC hdc, style_t *style) {
    if (!style) return;
    // Simple color conversion: 0xRRGGBB -> RGB(R, G, B)
    // HTML often uses RGB, Win32 uses BGR (COLORREF is 0x00BBGGRR)
    // Assuming style->color is stored as 0xRRGGBB
    
    int r = (style->color >> 16) & 0xFF;
    int g = (style->color >> 8) & 0xFF;
    int b = style->color & 0xFF;
    SetTextColor(hdc, RGB(r, g, b));

    // Background is tricky because we draw boxes, not just set BKColor
}

void render_tree(HDC hdc, layout_box_t *box, int offset_x, int offset_y) {
    if (!box) return;

    int x = box->dimensions.x + offset_x;
    int y = box->dimensions.y + offset_y;
    int w = box->dimensions.width;
    int h = box->dimensions.height;

    // Draw background if not transparent/white default (optimization)
    // For now, just draw a border for debugging if it's a block
    if (box->node->type == DOM_NODE_ELEMENT) {
        // Simple visual debug: Draw border for all blocks to see layout
        HBRUSH hBrush = GetStockObject(NULL_BRUSH);
        HBRUSH oldBrush = SelectObject(hdc, hBrush);
        HPEN hPen = CreatePen(PS_SOLID, 1, RGB(200, 200, 200));
        HPEN oldPen = SelectObject(hdc, hPen);
        
        Rectangle(hdc, x, y, x + w, y + h);
        
        SelectObject(hdc, oldPen);
        SelectObject(hdc, oldBrush);
        DeleteObject(hPen);
    } else if (box->node->type == DOM_NODE_TEXT && box->node->content) {
        set_color_from_style(hdc, box->node->style);
        SetBkMode(hdc, TRANSPARENT);
        
        RECT r = {x, y, x + w, y + h};
        // DT_WORDBREAK is essential for wrapping
        DrawText(hdc, box->node->content, -1, &r, DT_LEFT | DT_WORDBREAK | DT_NOPREFIX);
    }

    layout_box_t *child = box->first_child;
    while (child) {
        render_tree(hdc, child, offset_x, offset_y);
        child = child->next_sibling;
    }
}
