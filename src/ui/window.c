#include "window.h"
#include <commctrl.h>
#include <wininet.h>
#include <stdio.h>
#include "network/http.h"
#include "core/html.h"
#include "core/style.h"
#include "core/layout.h"
#include "core/log.h"
#include "ui/history.h"
#include "ui/bookmarks.h"
#include "ui/render.h"
#include "ui/form.h"

#define ID_BTN_STAR 101
#define ID_EDIT_URL 102
#define ID_BTN_GO   103
#define ID_CONTENT  104
#define ID_HISTORY  105
#define ID_LOADING_PANEL 106
#define ID_ANIM_CTRL     107
#define ID_PROG_CTRL     108
#define ID_STATUS_TEXT   109

#define TOP_BAR_HEIGHT 30
#define HISTORY_HEIGHT 150

// Standard Shell32 Animation IDs
#define IDR_AVI_FILECOPY 160

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
static LRESULT CALLBACK ContentWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
static LRESULT CALLBACK HistoryWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
static void ResizeChildWindows(HWND hwnd, int width, int height);
static void Navigate(HWND hwnd, const char *url);
static void HandleClick(HWND hwnd, int x, int y);

static history_tree_t *g_history = NULL;
static layout_box_t *g_current_layout = NULL;
static node_t *g_current_dom = NULL;
node_t *g_focused_node = NULL;
static char g_current_url[2048] = {0};
static HMODULE g_hShell32 = NULL;
static int g_skip_history = 0;

// Scrollbar State
static int g_scroll_x = 0;
static int g_scroll_y = 0;
static int g_content_width = 0;
static int g_content_height = 0;

static void UpdateScrollBars(HWND hwnd) {
    RECT rc;
    GetClientRect(hwnd, &rc);
    int client_w = rc.right - rc.left;
    int client_h = rc.bottom - rc.top;

    SCROLLINFO si;
    si.cbSize = sizeof(SCROLLINFO);
    si.fMask = SIF_RANGE | SIF_PAGE | SIF_POS;

    // Vertical
    si.nMin = 0;
    si.nMax = g_content_height;
    si.nPage = client_h;
    si.nPos = g_scroll_y;
    SetScrollInfo(hwnd, SB_VERT, &si, TRUE);

    // Horizontal
    si.nMin = 0;
    si.nMax = g_content_width;
    si.nPage = client_w;
    si.nPos = g_scroll_x;
    SetScrollInfo(hwnd, SB_HORZ, &si, TRUE);
}

BOOL CreateMainWindow(HINSTANCE hInstance, int nCmdShow) {
    g_history = history_create();
    
    // Initialize Common Controls for Animation and Progress Bar
    INITCOMMONCONTROLSEX icex;
    icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
    icex.dwICC = ICC_ANIMATE_CLASS | ICC_BAR_CLASSES | ICC_STANDARD_CLASSES;
    InitCommonControlsEx(&icex);

    g_hShell32 = LoadLibrary("shell32.dll");

    // Register Main Window Class
    const char className[] = "Gem32BrowserClass";
    WNDCLASS wc = {0};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = className;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);

    if (!RegisterClass(&wc)) {
        LOG_ERROR("Failed to register main window class");
        return FALSE;
    }

    // Register Content Window Class
    const char contentClassName[] = "Gem32ContentClass";
    WNDCLASS wcc = {0};
    wcc.lpfnWndProc = ContentWndProc;
    wcc.hInstance = hInstance;
    wcc.lpszClassName = contentClassName;
    wcc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wcc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1); // White background

    if (!RegisterClass(&wcc)) {
        LOG_ERROR("Failed to register content window class");
        return FALSE;
    }

    // Register History Window Class
    const char historyClassName[] = "Gem32HistoryClass";
    WNDCLASS whc = {0};
    whc.lpfnWndProc = HistoryWndProc;
    whc.hInstance = hInstance;
    whc.lpszClassName = historyClassName;
    whc.hCursor = LoadCursor(NULL, IDC_ARROW);
    whc.hbrBackground = GetSysColorBrush(COLOR_BTNFACE);

    if (!RegisterClass(&whc)) {
        LOG_ERROR("Failed to register history window class");
        return FALSE;
    }

    HWND hwnd = CreateWindowEx(
        0,
        className,
        "Gem32 Browser",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 800, 600,
        NULL, NULL, hInstance, NULL
    );

    if (hwnd == NULL) {
        LOG_ERROR("Failed to create main window");
        return FALSE;
    }

    LOG_INFO("Main window created successfully");

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    return TRUE;
}

#include "network/loader.h"

static void LoaderProgressCallback(int current, int total, void *ctx) {
    HWND hLoadingPanel = (HWND)ctx;
    HWND hProgress = GetDlgItem(hLoadingPanel, ID_PROG_CTRL);
    
    if (total > 0) {
        SendMessage(hProgress, PBM_SETRANGE, 0, MAKELPARAM(0, total));
        SendMessage(hProgress, PBM_SETPOS, current, 0);
    }
    
    // Pump messages to keep UI responsive and ANIMATION playing
    MSG msg;
    while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
}

static void FetchFavicon(history_node_t *node) {
    if (!node || !node->url) return;
    
    URL_COMPONENTS urlComp = {0};
    urlComp.dwStructSize = sizeof(urlComp);
    char host[256] = {0};
    urlComp.lpszHostName = host;
    urlComp.dwHostNameLength = sizeof(host);
    urlComp.dwSchemeLength = 1;

    if (!InternetCrackUrl(node->url, 0, 0, &urlComp)) return;

    if (strncmp(node->url, "gemini://", 9) == 0) return;

    char favicon_url[2048];
    snprintf(favicon_url, sizeof(favicon_url), "%s://%s/favicon.ico", 
             (urlComp.nScheme == INTERNET_SCHEME_HTTPS) ? "https" : "http", host);

    network_response_t *res = network_fetch(favicon_url);
    if (res && res->data && res->size > 0) {
        history_node_set_favicon(node, res->data, res->size);
        res->data = NULL;
    }
    if (res) network_response_free(res);
}

static void DrawHistoryTree(HDC hdc, history_node_t *node, int *x, int y) {
    if (!node) return;
    
    int iconSize = 16;
    int spacing = 24;
    int curX = *x;

    if (node->favicon_data) {
        render_image_data(hdc, node->favicon_data, node->favicon_size, curX, y, iconSize, iconSize);
    } else {
        // Draw a globe-ish circle for Gemini or missing icons
        HBRUSH hBrush = CreateSolidBrush(RGB(100, 150, 255));
        HBRUSH oldBrush = SelectObject(hdc, hBrush);
        Ellipse(hdc, curX, y, curX + iconSize, y + iconSize);
        SelectObject(hdc, oldBrush);
        DeleteObject(hBrush);
    }

    if (node == g_history->current) {
        HPEN hPen = CreatePen(PS_SOLID, 2, RGB(255, 0, 0));
        HPEN oldPen = SelectObject(hdc, hPen);
        HBRUSH oldBrush = SelectObject(hdc, GetStockObject(NULL_BRUSH));
        Rectangle(hdc, curX - 2, y - 2, curX + iconSize + 2, y + iconSize + 2);
        SelectObject(hdc, oldBrush);
        SelectObject(hdc, oldPen);
        DeleteObject(hPen);
    }

    *x += spacing;

    for (int i = 0; i < node->children_count; i++) {
        DrawHistoryTree(hdc, node->children[i], x, y);
    }
}

static history_node_t* HitTestHistory(history_node_t *node, int *x, int y, int hitX, int hitY) {
    if (!node) return NULL;
    int iconSize = 16;
    int spacing = 24;
    int curX = *x;

    if (hitX >= curX && hitX <= curX + iconSize && hitY >= y && hitY <= y + iconSize) {
        return node;
    }

    *x += spacing;

    for (int i = 0; i < node->children_count; i++) {
        history_node_t *res = HitTestHistory(node->children[i], x, y, hitX, hitY);
        if (res) return res;
    }
    return NULL;
}

static LRESULT CALLBACK HistoryWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_LBUTTONDOWN: {
            if (!g_history || !g_history->root) return 0;
            int hitX = LOWORD(lParam);
            int hitY = HIWORD(lParam);
            int x = 10;
            history_node_t *node = HitTestHistory(g_history->root, &x, 10, hitX, hitY);
            if (node) {
                g_history->current = node;
                g_skip_history = 1;
                Navigate(GetParent(hwnd), node->url);
                g_skip_history = 0;
            }
            return 0;
        }
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            if (g_history && g_history->root) {
                int x = 10;
                DrawHistoryTree(hdc, g_history->root, &x, 10);
            }
            EndPaint(hwnd, &ps);
            return 0;
        }
        default:
            return DefWindowProc(hwnd, msg, wParam, lParam);
    }
}

static void ProcessNewContent(HWND hContent, network_response_t *res, const char *url) {
    if (!res || !res->data) {
        LOG_WARN("ProcessNewContent called with null resource or data");
        return;
    }

    LOG_INFO("Processing new content for URL: %s (%lu bytes)", url, (unsigned long)res->size);

    HWND hMain = GetParent(hContent);
    HWND hLoadingPanel = GetDlgItem(hMain, ID_LOADING_PANEL);
    HWND hAnim = GetDlgItem(hLoadingPanel, ID_ANIM_CTRL);
    HWND hProg = GetDlgItem(hLoadingPanel, ID_PROG_CTRL);

    // Show Loading Panel, Hide Content
    ShowWindow(hContent, SW_HIDE);
    ShowWindow(hLoadingPanel, SW_SHOW);
    
    // Start Animation
    if (g_hShell32) {
        if (SendMessage(hAnim, ACM_OPEN, (WPARAM)g_hShell32, (LPARAM)MAKEINTRESOURCE(IDR_AVI_FILECOPY))) {
            SendMessage(hAnim, ACM_PLAY, (WPARAM)-1, MAKELPARAM(0, -1)); // Loop forever
        }
    }
    SendMessage(hProg, PBM_SETPOS, 0, 0);

    // Free previous DOM and Layout
    if (g_current_layout) {
        layout_free(g_current_layout);
        g_current_layout = NULL;
    }
    if (g_current_dom) {
        node_free(g_current_dom);
        g_current_dom = NULL;
    }

    strncpy(g_current_url, url, sizeof(g_current_url)-1);
    SetWindowText(GetDlgItem(hMain, ID_EDIT_URL), g_current_url);

    g_current_dom = html_parse(res->data);
    if (g_current_dom) {
        style_compute(g_current_dom);
        
        int total = loader_count_resources(g_current_dom);
        int current = 0;
        
        // Init bar
        SendMessage(hProg, PBM_SETRANGE, 0, MAKELPARAM(0, total > 0 ? total : 1));
        SendMessage(hProg, PBM_SETPOS, 0, 0);
        
        // Fetch resources (blocking with message pump)
        loader_fetch_resources(g_current_dom, url, LoaderProgressCallback, hLoadingPanel, &current, total);

        RECT rect;
        GetClientRect(hContent, &rect);
        g_current_layout = layout_create_tree(g_current_dom, rect.right - rect.left);

        // Update content size and scrollbars
        g_content_width = g_current_layout->fragment.border_box.width;
        g_content_height = g_current_layout->fragment.border_box.height;
        g_scroll_x = 0;
        g_scroll_y = 0;
        UpdateScrollBars(hContent);

        if (!g_skip_history) {
            history_add(g_history, url, "Title Placeholder");
            FetchFavicon(g_history->current);
        }
        InvalidateRect(GetDlgItem(hMain, ID_HISTORY), NULL, TRUE);
    }

    // Stop Animation, Hide Loading, Show Content
    SendMessage(hAnim, ACM_STOP, 0, 0);
    ShowWindow(hLoadingPanel, SW_HIDE);
    ShowWindow(hContent, SW_SHOW);
    
    // Trigger repaint
    InvalidateRect(hContent, NULL, TRUE);
    UpdateWindow(hContent);
}

static void Navigate(HWND hwnd, const char *url) {
    LOG_INFO("Navigating to: %s", url);
    
    // Could show a "Connecting..." status here if needed, but network_fetch is blocking
    // and usually faster than resource loading.
    
    network_response_t *res = network_fetch(url);
    if (res) {
        ProcessNewContent(GetDlgItem(hwnd, ID_CONTENT), res, res->final_url ? res->final_url : url);
        network_response_free(res);
    } else {
        LOG_ERROR("Failed to fetch URL: %s", url);
        MessageBox(hwnd, "Failed to fetch URL", "Error", MB_ICONERROR);
    }
}

static void HandleClick(HWND hContent, int x, int y) {
    if (!g_current_layout) return;

    // Adjust for scroll
    layout_box_t *hit = layout_hit_test(g_current_layout, x + g_scroll_x, y + g_scroll_y);
    if (!hit || !hit->node) {
        g_focused_node = NULL;
        return;
    }

    node_t *node = hit->node;
    while (node && node->type == DOM_NODE_TEXT) node = node->parent;

    // Check for anchor tag
    node_t *anchor = node;
    while (anchor) {
        if (anchor->type == DOM_NODE_ELEMENT && anchor->tag_name && strcasecmp(anchor->tag_name, "a") == 0) {
            const char *href = node_get_attr(anchor, "href");
            if (href) {
                char full_url[2048];
                DWORD len = sizeof(full_url);
                if (InternetCombineUrl(g_current_url, href, full_url, &len, 0)) {
                    Navigate(GetParent(hContent), full_url);
                } else {
                    Navigate(GetParent(hContent), href);
                }
                return;
            }
        }
        anchor = anchor->parent;
    }

    if (node && node->type == DOM_NODE_ELEMENT && node->tag_name) {
        int is_editable = 0;
        if (strcasecmp(node->tag_name, "textarea") == 0) is_editable = 1;
        else if (strcasecmp(node->tag_name, "input") == 0) {
            const char *type = node_get_attr(node, "type");
            if (!type || (strcasecmp(type, "text") == 0 || strcasecmp(type, "password") == 0 ||
                          strcasecmp(type, "email") == 0 || strcasecmp(type, "search") == 0 ||
                          strcasecmp(type, "tel") == 0 || strcasecmp(type, "url") == 0)) {
                is_editable = 1;
            }
        }

        if (is_editable) {
            g_focused_node = node;
            SetFocus(hContent);
        } else {
            g_focused_node = NULL;
        }

        int is_submit = 0;
        if (strcasecmp(node->tag_name, "button") == 0) is_submit = 1;
        else if (strcasecmp(node->tag_name, "input") == 0) {
             const char *type = node_get_attr(node, "type");
             if (type && (strcasecmp(type, "submit") == 0 || strcasecmp(type, "button") == 0)) {
                 is_submit = 1;
             }
        }

        if (is_submit) {
            char target_url[2048];
            network_response_t *res = form_submit(node, g_current_url, target_url, sizeof(target_url));
            if (res) {
                ProcessNewContent(hContent, res, target_url);
                network_response_free(res);
            }
        }
    }
}

static LRESULT CALLBACK ContentWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_LBUTTONDOWN:
            HandleClick(hwnd, LOWORD(lParam), HIWORD(lParam));
            break;
        case WM_SIZE:
            UpdateScrollBars(hwnd);
            return 0;
        case WM_VSCROLL: {
            int action = LOWORD(wParam);
            int newPos = g_scroll_y;
            SCROLLINFO si = { sizeof(SCROLLINFO), SIF_ALL, 0, 0, 0, 0, 0 };
            GetScrollInfo(hwnd, SB_VERT, &si);
            int page = si.nPage;
            switch (action) {
                case SB_LINEUP: newPos -= 20; break;
                case SB_LINEDOWN: newPos += 20; break;
                case SB_PAGEUP: newPos -= page; break;
                case SB_PAGEDOWN: newPos += page; break;
                case SB_THUMBTRACK: newPos = HIWORD(wParam); break;
            }
            if (newPos < 0) newPos = 0;
            if (newPos > g_content_height - (int)page) newPos = g_content_height - (int)page;
            if (newPos < 0) newPos = 0;
            if (newPos != g_scroll_y) {
                g_scroll_y = newPos;
                SetScrollPos(hwnd, SB_VERT, g_scroll_y, TRUE);
                InvalidateRect(hwnd, NULL, TRUE);
            }
            return 0;
        }
        case WM_HSCROLL: {
            int action = LOWORD(wParam);
            int newPos = g_scroll_x;
            SCROLLINFO si = { sizeof(SCROLLINFO), SIF_ALL, 0, 0, 0, 0, 0 };
            GetScrollInfo(hwnd, SB_HORZ, &si);
            int page = si.nPage;
            switch (action) {
                case SB_LINEUP: newPos -= 20; break;
                case SB_LINEDOWN: newPos += 20; break;
                case SB_PAGEUP: newPos -= page; break;
                case SB_PAGEDOWN: newPos += page; break;
                case SB_THUMBTRACK: newPos = HIWORD(wParam); break;
            }
            if (newPos < 0) newPos = 0;
            if (newPos > g_content_width - (int)page) newPos = g_content_width - (int)page;
            if (newPos < 0) newPos = 0;
            if (newPos != g_scroll_x) {
                g_scroll_x = newPos;
                SetScrollPos(hwnd, SB_HORZ, g_scroll_x, TRUE);
                InvalidateRect(hwnd, NULL, TRUE);
            }
            return 0;
        }
        case WM_MOUSEWHEEL: {
            int delta = (short)HIWORD(wParam);
            int scrollAmt = -(delta / 120) * 60;
            int newPos = g_scroll_y + scrollAmt;
            RECT rc; GetClientRect(hwnd, &rc);
            int max = g_content_height - (rc.bottom - rc.top);
            if (newPos < 0) newPos = 0;
            if (newPos > max) newPos = max;
            if (newPos < 0) newPos = 0;
            if (newPos != g_scroll_y) {
                g_scroll_y = newPos;
                SetScrollPos(hwnd, SB_VERT, g_scroll_y, TRUE);
                InvalidateRect(hwnd, NULL, TRUE);
            }
            return 0;
        }
        case WM_CHAR:
            if (g_focused_node) {
                char c = (char)wParam;
                if (c == '\b') { // Backspace
                    if (g_focused_node->current_value) {
                        size_t len = strlen(g_focused_node->current_value);
                        if (len > 0) g_focused_node->current_value[len - 1] = '\0';
                    }
                } else if (c >= 32) { // Printable chars
                    size_t len = g_focused_node->current_value ? strlen(g_focused_node->current_value) : 0;
                    char *new_val = realloc(g_focused_node->current_value, len + 2);
                    if (new_val) {
                        new_val[len] = c;
                        new_val[len + 1] = '\0';
                        g_focused_node->current_value = new_val;
                    }
                }
                InvalidateRect(hwnd, NULL, TRUE);
            }
            break;
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            HFONT hFont = GetStockObject(DEFAULT_GUI_FONT);
            SelectObject(hdc, hFont);
            if (g_current_layout) {
                render_tree(hdc, g_current_layout, -g_scroll_x, -g_scroll_y);
            } else {
                TextOut(hdc, 10, 10, "No content loaded.", 18);
            }
            EndPaint(hwnd, &ps);
            return 0;
        }
        default:
            return DefWindowProc(hwnd, msg, wParam, lParam);
    }
}

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE: {
            CreateWindow("BUTTON", "*", WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON, 0, 0, 0, 0, hwnd, (HMENU)ID_BTN_STAR, ((LPCREATESTRUCT)lParam)->hInstance, NULL);
            CreateWindow("EDIT", "http://frogfind.com", WS_VISIBLE | WS_CHILD | WS_BORDER | ES_AUTOHSCROLL, 0, 0, 0, 0, hwnd, (HMENU)ID_EDIT_URL, ((LPCREATESTRUCT)lParam)->hInstance, NULL);
            CreateWindow("BUTTON", "Go", WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON, 0, 0, 0, 0, hwnd, (HMENU)ID_BTN_GO, ((LPCREATESTRUCT)lParam)->hInstance, NULL);

            // Loading Panel (Container)
            HWND hLoading = CreateWindow("STATIC", "", WS_CHILD | WS_BORDER | SS_NOTIFY, 0, 0, 300, 150, hwnd, (HMENU)ID_LOADING_PANEL, ((LPCREATESTRUCT)lParam)->hInstance, NULL);
            // Animation Control
            CreateWindow(ANIMATE_CLASS, NULL, WS_CHILD | WS_VISIBLE | ACS_CENTER | ACS_TRANSPARENT, 10, 10, 280, 80, hLoading, (HMENU)ID_ANIM_CTRL, ((LPCREATESTRUCT)lParam)->hInstance, NULL);
            // Status Text
            CreateWindow("STATIC", "Downloading resources...", WS_CHILD | WS_VISIBLE | SS_CENTER, 10, 95, 280, 20, hLoading, (HMENU)ID_STATUS_TEXT, ((LPCREATESTRUCT)lParam)->hInstance, NULL);
            // Progress Bar
            CreateWindow(PROGRESS_CLASS, NULL, WS_CHILD | WS_VISIBLE | PBS_SMOOTH, 10, 120, 280, 20, hLoading, (HMENU)ID_PROG_CTRL, ((LPCREATESTRUCT)lParam)->hInstance, NULL);

            CreateWindow("Gem32ContentClass", NULL, WS_VISIBLE | WS_CHILD | WS_BORDER | WS_VSCROLL | WS_HSCROLL, 0, 0, 0, 0, hwnd, (HMENU)ID_CONTENT, ((LPCREATESTRUCT)lParam)->hInstance, NULL);
                        CreateWindow(
                            "Gem32HistoryClass", NULL,
                            WS_VISIBLE | WS_CHILD | WS_BORDER,
                            0, 0, 0, 0,
                            hwnd, (HMENU)ID_HISTORY,
                            ((LPCREATESTRUCT)lParam)->hInstance, NULL
                        );
            break;
        }
        case WM_COMMAND:
            if (LOWORD(wParam) == ID_BTN_GO) {
                char url[1024];
                GetWindowText(GetDlgItem(hwnd, ID_EDIT_URL), url, sizeof(url));
                Navigate(hwnd, url);
            } else if (LOWORD(wParam) == ID_BTN_STAR) {
                char url[1024];
                GetWindowText(GetDlgItem(hwnd, ID_EDIT_URL), url, sizeof(url));
                bookmark_add(url, "Bookmark Title");
                MessageBox(hwnd, "Bookmarked!", "Success", MB_OK);
            }
            break;
        case WM_SIZE:
            ResizeChildWindows(hwnd, LOWORD(lParam), HIWORD(lParam));
            break;
        case WM_CLOSE:
            DestroyWindow(hwnd);
            break;
        case WM_DESTROY:
            if (g_hShell32) FreeLibrary(g_hShell32);
            PostQuitMessage(0);
            break;
        default:
            return DefWindowProc(hwnd, msg, wParam, lParam);
    }
    return 0;
}

static void ResizeChildWindows(HWND hwnd, int width, int height) {
    int topBarY = 0;
    int btnSize = TOP_BAR_HEIGHT;
    int urlX = btnSize;
    int urlWidth = width - (btnSize * 2);

    HWND hStar = GetDlgItem(hwnd, ID_BTN_STAR);
    HWND hUrl = GetDlgItem(hwnd, ID_EDIT_URL);
    HWND hGo = GetDlgItem(hwnd, ID_BTN_GO);
    HWND hContent = GetDlgItem(hwnd, ID_CONTENT);
    HWND hHistory = GetDlgItem(hwnd, ID_HISTORY);
    HWND hLoading = GetDlgItem(hwnd, ID_LOADING_PANEL);

    if (hStar) MoveWindow(hStar, 0, topBarY, btnSize, btnSize, TRUE);
    if (hUrl) MoveWindow(hUrl, urlX, topBarY, urlWidth, btnSize, TRUE);
    if (hGo) MoveWindow(hGo, width - btnSize, topBarY, btnSize, btnSize, TRUE);

    int contentY = TOP_BAR_HEIGHT;
    int historyY = height - HISTORY_HEIGHT;
    int contentHeight = historyY - contentY;
    if (contentHeight < 0) contentHeight = 0;

    if (hContent) MoveWindow(hContent, 0, contentY, width, contentHeight, TRUE);
    if (hHistory) MoveWindow(hHistory, 0, historyY, width, HISTORY_HEIGHT, TRUE);

    // Center Loading Panel
    if (hLoading) {
        int panelW = 300;
        int panelH = 150;
        int panelX = (width - panelW) / 2;
        int panelY = (height - panelH) / 2;
        MoveWindow(hLoading, panelX, panelY, panelW, panelH, TRUE);
    }
}