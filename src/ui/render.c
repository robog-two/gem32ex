#include "render.h"
#include <stdio.h>
#include <olectl.h>

static void set_color_from_style(HDC hdc, style_t *style) {
    if (!style) return;
    int r = (style->color >> 16) & 0xFF;
    int g = (style->color >> 8) & 0xFF;
    int b = style->color & 0xFF;
    SetTextColor(hdc, RGB(r, g, b));
}

static void render_image(HDC hdc, layout_box_t *box, int x, int y, int w, int h) {
    if (!box->node->image_data || box->node->image_size == 0) return;

    HGLOBAL hGlobal = GlobalAlloc(GMEM_MOVEABLE, box->node->image_size);
    if (!hGlobal) return;

    void *ptr = GlobalLock(hGlobal);
    memcpy(ptr, box->node->image_data, box->node->image_size);
    GlobalUnlock(hGlobal);

    IStream *pStream = NULL;
    if (CreateStreamOnHGlobal(hGlobal, TRUE, &pStream) == S_OK) {
        IPicture *pPicture = NULL;
        if (OleLoadPicture(pStream, box->node->image_size, FALSE, &IID_IPicture, (void**)&pPicture) == S_OK) {
            long hmWidth, hmHeight;
            pPicture->lpVtbl->get_Width(pPicture, &hmWidth);
            pPicture->lpVtbl->get_Height(pPicture, &hmHeight);

            pPicture->lpVtbl->Render(pPicture, hdc, x, y, w, h, 0, hmHeight, hmWidth, -hmHeight, NULL);
            pPicture->lpVtbl->Release(pPicture);
        }
        pStream->lpVtbl->Release(pStream);
    }
}

void render_tree(HDC hdc, layout_box_t *box, int offset_x, int offset_y) {
    if (!box) return;

    int x = box->dimensions.x + offset_x;
    int y = box->dimensions.y + offset_y;
    int w = box->dimensions.width;
    int h = box->dimensions.height;

    if (box->node->type == DOM_NODE_ELEMENT) {
        if (box->node->tag_name && strcasecmp(box->node->tag_name, "img") == 0) {
            render_image(hdc, box, x, y, w, h);
        } else {
             // Simple visual debug: Draw border for all blocks to see layout
            HBRUSH hBrush = GetStockObject(NULL_BRUSH);
            HBRUSH oldBrush = SelectObject(hdc, hBrush);
            HPEN hPen = CreatePen(PS_SOLID, 1, RGB(220, 220, 220));
            HPEN oldPen = SelectObject(hdc, hPen);
            
            Rectangle(hdc, x, y, x + w, y + h);
            
            SelectObject(hdc, oldPen);
            SelectObject(hdc, oldBrush);
            DeleteObject(hPen);
        }
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
