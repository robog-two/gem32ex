#include "render.h"
#include <stdio.h>
#include <olectl.h>
#include "core/platform.h"
#include "core/log.h"

#ifndef WINGDIPAPI
#define WINGDIPAPI __stdcall
#endif

// --- GDI+ Flat API Definitions ---
typedef struct GdiplusStartupInput {
    UINT32 GdiplusVersion;
    void* DebugEventCallback;
    BOOL SuppressBackgroundThread;
    BOOL SuppressExternalCodecs;
} GdiplusStartupInput;

typedef struct GdiplusStartupOutput {
    void* NotificationHook;
    void* NotificationUnhook;
} GdiplusStartupOutput;

typedef void* GpGraphics;
typedef void* GpImage;
typedef void* GpBitmap;
typedef int GpStatus;

typedef GpStatus (WINGDIPAPI *PFN_GdiplusStartup)(ULONG_PTR*, const GdiplusStartupInput*, GdiplusStartupOutput*);
typedef void (WINGDIPAPI *PFN_GdiplusShutdown)(ULONG_PTR);
typedef GpStatus (WINGDIPAPI *PFN_GdipCreateBitmapFromStream)(IStream*, GpBitmap**);
typedef GpStatus (WINGDIPAPI *PFN_GdipCreateFromHDC)(HDC, GpGraphics**);
typedef GpStatus (WINGDIPAPI *PFN_GdipDeleteGraphics)(GpGraphics*);
typedef GpStatus (WINGDIPAPI *PFN_GdipDisposeImage)(GpImage*);
typedef GpStatus (WINGDIPAPI *PFN_GdipDrawImageRectI)(GpGraphics*, GpImage*, INT, INT, INT, INT);

static HMODULE g_hGdiPlus = NULL;
static ULONG_PTR g_gdiplusToken = 0;
static PFN_GdiplusStartup fn_GdiplusStartup = NULL;
static PFN_GdiplusShutdown fn_GdiplusShutdown = NULL;
static PFN_GdipCreateBitmapFromStream fn_GdipCreateBitmapFromStream = NULL;
static PFN_GdipCreateFromHDC fn_GdipCreateFromHDC = NULL;
static PFN_GdipDeleteGraphics fn_GdipDeleteGraphics = NULL;
static PFN_GdipDisposeImage fn_GdipDisposeImage = NULL;
static PFN_GdipDrawImageRectI fn_GdipDrawImageRectI = NULL;

void render_init(void) {
    g_hGdiPlus = LoadLibrary("gdiplus.dll");
    if (g_hGdiPlus) {
        fn_GdiplusStartup = (PFN_GdiplusStartup)GetProcAddress(g_hGdiPlus, "GdiplusStartup");
        fn_GdiplusShutdown = (PFN_GdiplusShutdown)GetProcAddress(g_hGdiPlus, "GdiplusShutdown");
        fn_GdipCreateBitmapFromStream = (PFN_GdipCreateBitmapFromStream)GetProcAddress(g_hGdiPlus, "GdipCreateBitmapFromStream");
        fn_GdipCreateFromHDC = (PFN_GdipCreateFromHDC)GetProcAddress(g_hGdiPlus, "GdipCreateFromHDC");
        fn_GdipDeleteGraphics = (PFN_GdipDeleteGraphics)GetProcAddress(g_hGdiPlus, "GdipDeleteGraphics");
        fn_GdipDisposeImage = (PFN_GdipDisposeImage)GetProcAddress(g_hGdiPlus, "GdipDisposeImage");
        fn_GdipDrawImageRectI = (PFN_GdipDrawImageRectI)GetProcAddress(g_hGdiPlus, "GdipDrawImageRectI");

        if (fn_GdiplusStartup && fn_GdipCreateBitmapFromStream) {
            GdiplusStartupInput input = {1, NULL, FALSE, FALSE};
            if (fn_GdiplusStartup(&g_gdiplusToken, &input, NULL) != 0) {
                LOG_ERROR("GdiplusStartup failed");
                FreeLibrary(g_hGdiPlus);
                g_hGdiPlus = NULL;
            } else {
                LOG_INFO("GDI+ initialized successfully");
            }
        } else {
            LOG_ERROR("Could not locate required GDI+ functions");
            FreeLibrary(g_hGdiPlus);
            g_hGdiPlus = NULL;
        }
    } else {
        LOG_WARN("gdiplus.dll not found. PNG support disabled.");
    }
}

void render_cleanup(void) {
    if (g_hGdiPlus && fn_GdiplusShutdown) {
        fn_GdiplusShutdown(g_gdiplusToken);
        FreeLibrary(g_hGdiPlus);
        g_hGdiPlus = NULL;
    }
}

// Detect PNG signature (89 50 4E 47 0D 0A 1A 0A)
static int is_png(const void *data, size_t size) {
    if (size < 8) return 0;
    const unsigned char *bytes = (const unsigned char *)data;
    return bytes[0] == 0x89 && bytes[1] == 'P' && bytes[2] == 'N' && bytes[3] == 'G' &&
           bytes[4] == 0x0D && bytes[5] == 0x0A && bytes[6] == 0x1A && bytes[7] == 0x0A;
}

// Extract PNG dimensions from the IHDR chunk
// PNG format: 8 byte signature, then IHDR chunk with width/height at bytes 16-24
static int get_png_dimensions(const void *data, size_t size, int *out_width, int *out_height) {
    if (size < 24 || !is_png(data, size)) return 0;

    const unsigned char *bytes = (const unsigned char *)data;
    // IHDR chunk width is at offset 16-20 (big-endian)
    // IHDR chunk height is at offset 20-24 (big-endian)
    unsigned int width = (bytes[16] << 24) | (bytes[17] << 16) | (bytes[18] << 8) | bytes[19];
    unsigned int height = (bytes[20] << 24) | (bytes[21] << 16) | (bytes[22] << 8) | bytes[23];

    // Sanity check - PNG dimensions should be reasonable
    if (width == 0 || width > 65535 || height == 0 || height > 65535) return 0;

    *out_width = (int)width;
    *out_height = (int)height;
    return 1;
}

// Public function for extracting image dimensions (supports PNG, others)
void render_extract_image_dimensions(const void *data, size_t size, int *out_width, int *out_height) {
    if (!data || !out_width || !out_height) {
        if (out_width) *out_width = 0;
        if (out_height) *out_height = 0;
        return;
    }

    // Try PNG first
    if (get_png_dimensions(data, size, out_width, out_height)) {
        return;
    }

    // For other formats (JPG, BMP, GIF, etc.), we'd need format-specific parsers
    // For now, default to reasonable dimensions if format unknown
    *out_width = 100;
    *out_height = 100;
}

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

void render_image_data(HDC hdc, void *data, size_t size, int x, int y, int w, int h) {
    if (!data || size == 0) return;

    // Validate image size is reasonable (some sanity check)
    if (size > 10 * 1024 * 1024) {
        LOG_WARN("Image data too large: %lu bytes (max 10MB)", (unsigned long)size);
        return;
    }

    int is_png_file = is_png(data, size);

    HGLOBAL hGlobal = GlobalAlloc(GMEM_MOVEABLE, size);
    if (!hGlobal) {
        LOG_WARN("GlobalAlloc failed for image data size %lu", (unsigned long)size);
        return;
    }

    void *ptr = GlobalLock(hGlobal);
    if (!ptr) {
        GlobalFree(hGlobal);
        LOG_WARN("GlobalLock failed for image data");
        return;
    }

    memcpy(ptr, data, size);
    GlobalUnlock(hGlobal);

    IStream *pStream = NULL;
    if (CreateStreamOnHGlobal(hGlobal, TRUE, &pStream) == S_OK) {
        int drawn = 0;

        // Try GDI+ first (supports PNG, JPEG, GIF, BMP, TIFF, ICO)
        if (g_hGdiPlus && fn_GdipCreateBitmapFromStream && fn_GdipCreateFromHDC) {
            GpBitmap *bitmap = NULL;
            GpStatus status = fn_GdipCreateBitmapFromStream(pStream, &bitmap);
            if (status == 0 && bitmap) {
                GpGraphics *graphics = NULL;
                if (fn_GdipCreateFromHDC(hdc, &graphics) == 0 && graphics) {
                    if (fn_GdipDrawImageRectI(graphics, (GpImage*)bitmap, x, y, w, h) == 0) {
                        drawn = 1;
                        LOG_DEBUG("Image rendered with GDI+ (%lu bytes)", (unsigned long)size);
                    }
                    fn_GdipDeleteGraphics(graphics);
                }
                fn_GdipDisposeImage((GpImage*)bitmap);
            } else if (status != 0) {
                if (is_png_file) {
                    // PNG codec issue - likely GDI+ version or installation problem
                    // GdiplusStatus codes: 1=GenericError, 2=InvalidParameter, 4=NotImplemented, etc.
                    LOG_WARN("GDI+ PNG codec failed (status %d). This is usually caused by:", (int)status);
                    LOG_WARN("  1) GDI+ not installed or outdated (requires update KB976519)");
                    LOG_WARN("  2) PNG with unusual compression or color profile");
                    LOG_WARN("  3) Interlaced PNG format not supported");
                    LOG_WARN("  Workaround: Update gdiplus.dll or convert PNG to JPG/BMP");
                } else {
                    LOG_DEBUG("GDI+ failed to load image (status %d), trying OleLoadPicture", (int)status);
                }
            }
            // If GDI+ failed, reset stream position for fallback
            LARGE_INTEGER li = {0};
            pStream->lpVtbl->Seek(pStream, li, STREAM_SEEK_SET, NULL);
        }

        if (!drawn && !is_png_file) {
            // Fallback to OLE (BMP, JPG, GIF, ICO) - NOTE: OLE does NOT support PNG
            IPicture *pPicture = NULL;
            HRESULT hr = OleLoadPicture(pStream, size, FALSE, &IID_IPicture, (void**)&pPicture);
            if (hr == S_OK && pPicture) {
                long hmWidth, hmHeight;
                pPicture->lpVtbl->get_Width(pPicture, &hmWidth);
                pPicture->lpVtbl->get_Height(pPicture, &hmHeight);

                HRESULT render_hr = pPicture->lpVtbl->Render(pPicture, hdc, x, y, w, h, 0, hmHeight, hmWidth, -hmHeight, NULL);
                if (render_hr == S_OK) {
                    drawn = 1;
                    LOG_DEBUG("Image rendered with OleLoadPicture (%lu bytes)", (unsigned long)size);
                } else {
                    LOG_WARN("OleLoadPicture Render failed (hr=0x%lx)", (unsigned long)render_hr);
                }
                pPicture->lpVtbl->Release(pPicture);
            } else {
                LOG_WARN("Image load failed (OleLoadPicture) for size %lu (hr=0x%lx)", (unsigned long)size, (unsigned long)hr);
            }
        }

        if (!drawn && is_png_file) {
            LOG_ERROR("PNG image could not be rendered (size %lu bytes)", (unsigned long)size);
            LOG_ERROR("PNG support requires GDI+ to be properly installed on Windows XP SP3");
            LOG_ERROR("Solutions: 1) Install Windows GDI+ update KB976519");
            LOG_ERROR("          2) Download and run gdiplus_redistributable from Microsoft");
            LOG_ERROR("          3) Convert PNG images to JPG or BMP format");
        }

        pStream->lpVtbl->Release(pStream);
    } else {
        LOG_ERROR("CreateStreamOnHGlobal failed for image data (size %lu)", (unsigned long)size);
        GlobalFree(hGlobal);
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
            render_image_data(hdc, box->node->image_data, box->node->image_size, x, y, w, h);
        } else {
            // Background color if set
            if (box->node->style->bg_color != 0xFFFFFF) {
                HBRUSH hBrush = CreateSolidBrush(RGB((box->node->style->bg_color >> 16) & 0xFF, (box->node->style->bg_color >> 8) & 0xFF, box->node->style->bg_color & 0xFF));
                RECT r = {x, y, x + w, y + h};
                FillRect(hdc, &r, hBrush);
                DeleteObject(hBrush);
            }

            // Background Image
            if (box->node->bg_image_data) {
                render_image_data(hdc, box->node->bg_image_data, box->node->bg_image_size, x, y, w, h);
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

            // Render iframe content
            if (box->iframe_root) {
                int bw = box->node->style->border_width;
                int pl = box->node->style->padding_left;
                int pt = box->node->style->padding_top;
                int cx = x + bw + pl;
                int cy = y + bw + pt;
                int cw = box->fragment.content_box.width;
                // Use content box height if calculated, otherwise calculate from border box
                int ch = box->fragment.content_box.height;
                if (ch <= 0) ch = h - (bw * 2) - pt - box->node->style->padding_bottom;
                
                HRGN hRgn = CreateRectRgn(cx, cy, cx + cw, cy + ch);
                SelectClipRgn(hdc, hRgn);
                
                render_tree(hdc, box->iframe_root, cx, cy);
                
                SelectClipRgn(hdc, NULL);
                DeleteObject(hRgn);
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
    // Do not render children of replaced elements (img, iframe)
    int is_replaced = 0;
    if (box->node->type == DOM_NODE_ELEMENT && box->node->tag_name) {
        if (strcasecmp(box->node->tag_name, "img") == 0 || 
            strcasecmp(box->node->tag_name, "iframe") == 0) {
            is_replaced = 1;
        }
    }

    if (!is_replaced) {
        layout_box_t *child = box->first_child;
        while (child) {
            render_tree(hdc, child, x, y);
            child = child->next_sibling;
        }
    }
}