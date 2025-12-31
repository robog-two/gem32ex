#include "render.h"
#include <stdio.h>
#include <olectl.h>
#include "core/platform.h"

// Implementation of core/platform.h interface
void platform_measure_text(const char *text, style_t *style, int width_constraint, int *out_width, int *out_height) {
    if (!text || !out_width || !out_height) return;

    // Create a temporary DC for measurement
    HDC hdc = CreateCompatibleDC(NULL);
    
    // Select default font (same as Render)
    HFONT hFont = GetStockObject(DEFAULT_GUI_FONT);
    HFONT oldFont = SelectObject(hdc, hFont);

    RECT r = {0, 0, 0, 0};
    if (width_constraint > 0) {
        r.right = width_constraint;
    }

    UINT format = DT_LEFT | DT_NOPREFIX;
    if (width_constraint > 0) {
        format |= DT_WORDBREAK;
    } else {
        format |= DT_CALCRECT; // Just expands right
    }
    
    // Calculate
    DrawText(hdc, text, -1, &r, format | DT_CALCRECT);

    *out_width = r.right - r.left;
    *out_height = r.bottom - r.top;

    SelectObject(hdc, oldFont);
    DeleteDC(hdc);
}

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
    if (!box || (box->node && box->node->style && box->node->style->display == DISPLAY_NONE)) return;

    int x = box->fragment.border_box.x + offset_x;
    int y = box->fragment.border_box.y + offset_y;
    int w = box->fragment.border_box.width;
    int h = box->fragment.border_box.height;

    if (box->node->type == DOM_NODE_ELEMENT) {
        if (box->node->tag_name && strcasecmp(box->node->tag_name, "img") == 0) {
            render_image(hdc, box, x, y, w, h);
        } else {
            // Background color if set
            if (box->node->style->bg_color != 0xFFFFFF) {
                HBRUSH hBrush = CreateSolidBrush(RGB((box->node->style->bg_color >> 16) & 0xFF, (box->node->style->bg_color >> 8) & 0xFF, box->node->style->bg_color & 0xFF));
                RECT r = {x, y, x + w, y + h};
                FillRect(hdc, &r, hBrush);
                DeleteObject(hBrush);
            }
            
            // Border if set
            if (box->node->style->border_width > 0) {
                HPEN hPen = CreatePen(PS_SOLID, box->node->style->border_width, RGB(0, 0, 0));
                HPEN oldPen = SelectObject(hdc, hPen);
                HBRUSH oldBrush = SelectObject(hdc, GetStockObject(NULL_BRUSH));
                
                Rectangle(hdc, x, y, x + w, y + h);
                
                SelectObject(hdc, oldBrush);
                SelectObject(hdc, oldPen);
                DeleteObject(hPen);
            }
        }
    } else if (box->node->type == DOM_NODE_TEXT && box->node->content) {
        set_color_from_style(hdc, box->node->style);
        SetBkMode(hdc, TRANSPARENT);
        
        RECT r = {x, y, x + w, y + h};
        DrawText(hdc, box->node->content, -1, &r, DT_LEFT | DT_WORDBREAK | DT_NOPREFIX);
    }

    layout_box_t *child = box->first_child;
    while (child) {
        render_tree(hdc, child, offset_x, offset_y);
        child = child->next_sibling;
    }
}
