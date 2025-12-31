#include "render.h"
#include <stdio.h>
#include <olectl.h>
#include "core/platform.h"
#include "core/log.h"

static HFONT get_font(style_t *style) {
    int height = -12; // Default
    int weight = FW_NORMAL;
    DWORD italic = FALSE;
    DWORD underline = FALSE;
    DWORD strikeout = FALSE;
    const char *face = "Arial";
    DWORD pitchAndFamily = DEFAULT_PITCH | FF_DONTCARE;

    if (style) {
        if (style->font_size > 0) height = -style->font_size;
        if (style->font_weight >= 700) weight = FW_BOLD;
        if (style->font_style == FONT_STYLE_ITALIC) italic = TRUE;
        if (style->text_decoration == TEXT_DECORATION_UNDERLINE) underline = TRUE;
        if (style->text_decoration == TEXT_DECORATION_LINE_THROUGH) strikeout = TRUE;
        
        if (style->font_family == FONT_FAMILY_MONOSPACE) {
            face = "Courier New";
            pitchAndFamily = FIXED_PITCH | FF_MODERN;
        } else if (style->font_family == FONT_FAMILY_SERIF) {
            face = "Times New Roman";
            pitchAndFamily = VARIABLE_PITCH | FF_ROMAN;
        }
    }

    HFONT hFont = CreateFont(height, 0, 0, 0, weight, italic, underline, strikeout, 
                      DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, 
                      DEFAULT_QUALITY, pitchAndFamily, face);
    
    if (!hFont) {
        hFont = GetStockObject(DEFAULT_GUI_FONT);
    }
    return hFont;
}

// Implementation of core/platform.h interface
void platform_measure_text(const char *text, style_t *style, int width_constraint, int *out_width, int *out_height, int *out_baseline) {
    if (!text || !out_width || !out_height) return;

    // Create a temporary DC for measurement
    HDC hdc = CreateCompatibleDC(NULL);
    if (!hdc) return; // Hard failure check
    
    HFONT hFont = get_font(style);
    HFONT oldFont = (HFONT)SelectObject(hdc, hFont);

    TEXTMETRIC tm;
    if (GetTextMetrics(hdc, &tm)) {
        if (out_baseline) *out_baseline = tm.tmAscent;
    } else {
        if (out_baseline) *out_baseline = 12; // Fallback
    }

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
    DeleteObject(hFont);
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
        } else {
            LOG_ERROR("OleLoadPicture failed for image data of size %zu", box->node->image_size);
        }
        pStream->lpVtbl->Release(pStream);
    } else {
        LOG_ERROR("CreateStreamOnHGlobal failed for image data");
    }
}

void render_tree(HDC hdc, layout_box_t *box, int offset_x, int offset_y) {
    if (!box || !box->node || !box->node->style) return;
    if (box->node->style->display == DISPLAY_NONE) return;

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
                COLORREF borderColor = RGB(0, 0, 0);
                if (box->node == g_focused_node) borderColor = RGB(0, 120, 215); // Focused blue
                
                HPEN hPen = CreatePen(PS_SOLID, box->node->style->border_width, borderColor);
                HPEN oldPen = SelectObject(hdc, hPen);
                HBRUSH oldBrush = SelectObject(hdc, GetStockObject(NULL_BRUSH));
                
                Rectangle(hdc, x, y, x + w, y + h);
                
                SelectObject(hdc, oldBrush);
                SelectObject(hdc, oldPen);
                DeleteObject(hPen);
            }

            // Render input value
            if (box->node->tag_name && strcasecmp(box->node->tag_name, "input") == 0) {
                const char *val = box->node->current_value ? box->node->current_value : node_get_attr(box->node, "value");
                if (val && strlen(val) > 0) {
                    set_color_from_style(hdc, box->node->style);
                    SetBkMode(hdc, TRANSPARENT);
                    HFONT hFont = get_font(box->node->style);
                    HFONT oldFont = SelectObject(hdc, hFont);
                    
                    int bw = box->node->style->border_width;
                    RECT r = {
                        x + box->node->style->padding_left + bw, 
                        y + box->node->style->padding_top + bw, 
                        x + w - box->node->style->padding_right - bw, 
                        y + h - box->node->style->padding_bottom - bw
                    };
                    
                    UINT format = DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX;
                    if (box->node->style->text_align == TEXT_ALIGN_CENTER) format = DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX;
                    else if (box->node->style->text_align == TEXT_ALIGN_RIGHT) format = DT_RIGHT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX;
                    
                    DrawText(hdc, val, -1, &r, format);
                    
                    SelectObject(hdc, oldFont);
                    DeleteObject(hFont);
                }
            }
        }
    } else if (box->node->type == DOM_NODE_TEXT && box->node->content) {
        set_color_from_style(hdc, box->node->style);
        SetBkMode(hdc, TRANSPARENT);
        
        HFONT hFont = get_font(box->node->style);
        HFONT oldFont = SelectObject(hdc, hFont);

        RECT r = {x, y, x + w, y + h};
        DrawText(hdc, box->node->content, -1, &r, DT_LEFT | DT_WORDBREAK | DT_NOPREFIX);

        SelectObject(hdc, oldFont);
        DeleteObject(hFont);
    }

    // Pass the current box's absolute position as the offset for children
    // This assumes children's coordinates are relative to this box.
    layout_box_t *child = box->first_child;
    while (child) {
        render_tree(hdc, child, x, y);
        child = child->next_sibling;
    }
}
