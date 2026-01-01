#include "window.h"
#include <windows.h>
#include <commctrl.h>
#include <stdio.h>
#include "render.h"
#include "history.h"
#include "history_ui.h"
#include "bookmarks.h"
#include "core/log.h"
#include "network/protocol.h"
#include "core/html.h"
#include "core/layout.h"
#include "ui/form.h"

// Constants
#define TOP_BAR_HEIGHT 30
#define HISTORY_WIDTH 200

// Control IDs
#define ID_EDIT_URL 1001
#define ID_BTN_GO 1002
#define ID_CONTENT 1003
#define ID_HISTORY 1004
#define ID_BTN_STAR 1005
#define ID_LOADING_PANEL 1006
#define ID_ANIM_CTRL 1007
#define ID_STATUS_TEXT 1008
#define ID_PROG_CTRL 1009

// Global state
static layout_box_t *g_current_layout = NULL;
static history_tree_t *g_history = NULL;
static int g_scroll_y = 0;
static int g_scroll_x = 0;
static int g_content_height = 0;
static int g_content_width = 0;
node_t *g_focused_node = NULL; // Removed static, matches render.h
static HINSTANCE g_hInst;
static int g_manual_navigation = 0;
static int g_skip_history = 0;
static HMODULE g_hShell32 = NULL;
static HWND g_hLoading = NULL;

// Forward declarations
static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
static LRESULT CALLBACK LoadingWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
static LRESULT CALLBACK ContentWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
static LRESULT CALLBACK HistoryWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
static void ResizeChildWindows(HWND hwnd, int width, int height);
void Navigate(HWND hwnd, const char *url); // Forward declaration

BOOL CreateMainWindow(HINSTANCE hInstance, int nCmdShow) {
    g_hInst = hInstance;
    g_history = history_create(); // Fixed: history_create, not history_tree_create
    
    // Initialize Common Controls
    INITCOMMONCONTROLSEX icex;
    icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
    icex.dwICC = ICC_ANIMATE_CLASS | ICC_BAR_CLASSES;
    InitCommonControlsEx(&icex);
    
    // Register Main Window Class
    WNDCLASSEX wc = {0};
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = "Gem32WindowClass";
    if (!RegisterClassEx(&wc)) return FALSE;

    // Register Content Window Class
    WNDCLASSEX wcContent = {0};
    wcContent.cbSize = sizeof(WNDCLASSEX);
    wcContent.lpfnWndProc = ContentWndProc;
    wcContent.hInstance = hInstance;
    wcContent.hCursor = LoadCursor(NULL, IDC_IBEAM);
    wcContent.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wcContent.lpszClassName = "Gem32ContentClass";
    if (!RegisterClassEx(&wcContent)) return FALSE;

    // Register History Window Class
    WNDCLASSEX wcHistory = {0};
    wcHistory.cbSize = sizeof(WNDCLASSEX);
    wcHistory.lpfnWndProc = HistoryWndProc;
    wcHistory.hInstance = hInstance;
    wcHistory.hCursor = LoadCursor(NULL, IDC_ARROW);
    wcHistory.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wcHistory.lpszClassName = "Gem32HistoryClass";
    if (!RegisterClassEx(&wcHistory)) return FALSE;

    // Register Loading Window Class
    WNDCLASSEX wcLoading = {0};
    wcLoading.cbSize = sizeof(WNDCLASSEX);
    wcLoading.lpfnWndProc = LoadingWndProc;
    wcLoading.hInstance = hInstance;
    wcLoading.hCursor = LoadCursor(NULL, IDC_WAIT);
    wcLoading.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wcLoading.lpszClassName = "Gem32LoadingClass";
    if (!RegisterClassEx(&wcLoading)) return FALSE;

    // Create Main Window
    HWND hwnd = CreateWindow("Gem32WindowClass", "Gem32 Browser", WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 1024, 768, NULL, NULL, hInstance, NULL);
    if (!hwnd) return FALSE;

    // Create Loading Popup (Hidden initially)
    g_hLoading = CreateWindowEx(WS_EX_TOPMOST | WS_EX_TOOLWINDOW, "Gem32LoadingClass", "", WS_POPUP | WS_BORDER, 0, 0, 300, 150, hwnd, NULL, hInstance, NULL);
    
    // Animation Control
    CreateWindow(ANIMATE_CLASS, NULL, WS_CHILD | WS_VISIBLE | ACS_CENTER | ACS_TRANSPARENT, 10, 10, 280, 80, g_hLoading, (HMENU)ID_ANIM_CTRL, hInstance, NULL);
    // Status Text
    CreateWindow("STATIC", "Downloading resources...", WS_CHILD | WS_VISIBLE | SS_CENTER, 10, 95, 280, 20, g_hLoading, (HMENU)ID_STATUS_TEXT, hInstance, NULL);
    // Progress Bar
    CreateWindow(PROGRESS_CLASS, NULL, WS_CHILD | WS_VISIBLE | PBS_SMOOTH, 10, 120, 280, 20, g_hLoading, (HMENU)ID_PROG_CTRL, hInstance, NULL);

    g_hShell32 = LoadLibrary("shell32.dll");

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    return TRUE;
}

static void UpdateScrollBars(HWND hwnd) {
    RECT rc;
    GetClientRect(hwnd, &rc);
    int viewWidth = rc.right - rc.left;
    int viewHeight = rc.bottom - rc.top;

    SCROLLINFO si = {0};
    si.cbSize = sizeof(SCROLLINFO);
    si.fMask = SIF_PAGE | SIF_RANGE | SIF_POS;
    si.nMin = 0;

    // Vertical
    si.nMax = g_content_height;
    si.nPage = viewHeight;
    si.nPos = g_scroll_y;
    SetScrollInfo(hwnd, SB_VERT, &si, TRUE);

    // Horizontal
    si.nMax = g_content_width;
    si.nPage = viewWidth;
    si.nPos = g_scroll_x;
    SetScrollInfo(hwnd, SB_HORZ, &si, TRUE);
}

static void HandleClick(HWND hwnd, int x, int y) {
    int absoluteX = x + g_scroll_x;
    int absoluteY = y + g_scroll_y;

    if (!g_current_layout) return;

    // Fixed: layout_hit_test, not layout_find_at
    layout_box_t *clicked = layout_hit_test(g_current_layout, absoluteX, absoluteY);
    if (clicked && clicked->node) {
        node_t *node = clicked->node;

        // Focus management
        if (strcasecmp(node->tag_name, "input") == 0 || strcasecmp(node->tag_name, "textarea") == 0) {
            g_focused_node = node;
            SetFocus(hwnd); // Ensure window has keyboard focus
            InvalidateRect(hwnd, NULL, TRUE);
        } else {
            g_focused_node = NULL;
        }

        // Link handling
        if (strcasecmp(node->tag_name, "a") == 0) {
            const char *href = node_get_attr(node, "href");
            if (href) {
                Navigate(GetParent(hwnd), href);
            }
        }

        // Submit Button Handling
        int is_submit = 0;
        if (strcasecmp(node->tag_name, "button") == 0) is_submit = 1;
        else if (strcasecmp(node->tag_name, "input") == 0) {
             const char *type = node_get_attr(node, "type");
             if (type && (strcasecmp(type, "submit") == 0 || strcasecmp(type, "button") == 0)) {
                 is_submit = 1;
             }
        }

        if (is_submit) {
            const char *base_url = (g_history && g_history->current) ? g_history->current->url : "about:blank";
            char target_url[2048];
            
            // Show Loading
            if (g_hLoading) {
                RECT rcMain;
                GetWindowRect(GetParent(hwnd), &rcMain);
                int mainW = rcMain.right - rcMain.left;
                int mainH = rcMain.bottom - rcMain.top;
                int panelW = 300;
                int panelH = 150;
                int x = rcMain.left + (mainW - panelW) / 2;
                int y = rcMain.top + (mainH - panelH) / 2;
                SetWindowPos(g_hLoading, HWND_TOP, x, y, panelW, panelH, SWP_SHOWWINDOW);
                
                HWND hAnim = GetDlgItem(g_hLoading, ID_ANIM_CTRL);
                if (g_hShell32 && hAnim) {
                    if (SendMessage(hAnim, ACM_OPEN, (WPARAM)g_hShell32, (LPARAM)MAKEINTRESOURCE(IDR_AVI_FILECOPY))) {
                        SendMessage(hAnim, ACM_PLAY, (WPARAM)-1, MAKELPARAM(0, -1));
                    }
                }
                UpdateWindow(g_hLoading);
                MSG msg;
                while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
                    TranslateMessage(&msg);
                    DispatchMessage(&msg);
                }
            }

            network_response_t *res = form_submit(node, base_url, target_url, sizeof(target_url));
            if (res) {
                ProcessResponse(GetParent(hwnd), res, target_url);
                network_response_free(res);
            } else {
                LOG_ERROR("Form submission failed");
            }

            if (g_hLoading) {
                HWND hAnim = GetDlgItem(g_hLoading, ID_ANIM_CTRL);
                if (hAnim) SendMessage(hAnim, ACM_STOP, 0, 0);
                ShowWindow(g_hLoading, SW_HIDE);
            }
        }
    }
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

static LRESULT CALLBACK HistoryWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_LBUTTONDOWN: {
            if (!g_history || !g_history->root) return 0;
            int hitX = LOWORD(lParam);
            int hitY = HIWORD(lParam);
            history_node_t *node = history_ui_hit_test(g_history, hitX, hitY);
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
                history_ui_draw(hdc, g_history);
            }
            EndPaint(hwnd, &ps);
            return 0;
        }
        default:
            return DefWindowProc(hwnd, msg, wParam, lParam);
    }
}

// Standard Shell32 Animation IDs
#define IDR_AVI_FILECOPY 160

static void ProcessResponse(HWND hwnd, network_response_t *res, const char *url) {
    if (!res || !res->data) {
        LOG_ERROR("ProcessResponse: No data for %s", url);
        return;
    }

    if (!g_skip_history) {
        history_add(g_history, res->final_url ? res->final_url : url, "Title");
        history_ui_fetch_favicon(g_history->current);
    }

    node_t *new_dom = html_parse((char*)res->data);
    if (new_dom) {
        // Free old layout and DOM
        if (g_current_layout) layout_free(g_current_layout);
        
        style_compute(new_dom);

        // Fetch resources (Images, etc.)
        int resource_count = loader_count_resources(new_dom);
        
        // Initialize progress bar
        HWND hProg = GetDlgItem(g_hLoading, ID_PROG_CTRL);
        if (hProg) {
            SendMessage(hProg, PBM_SETRANGE, 0, MAKELPARAM(0, resource_count > 0 ? resource_count : 1));
            SendMessage(hProg, PBM_SETPOS, 0, 0);
        }

        if (resource_count > 0) {
             int current = 0;
             loader_fetch_resources(new_dom, res->final_url ? res->final_url : url, LoaderProgressCallback, g_hLoading, &current, resource_count);
        }

        RECT rc;
        GetClientRect(GetDlgItem(hwnd, ID_CONTENT), &rc);
        // Initialize constraint space properly
        constraint_space_t space = {0};
        space.available_width = rc.right - rc.left;
        space.is_fixed_width = 1;
        
        g_current_layout = layout_create_tree(new_dom, space.available_width);

        if (g_current_layout) {
            g_content_height = g_current_layout->fragment.border_box.height;
            g_content_width = g_current_layout->fragment.border_box.width;
        } else {
            g_content_height = 0;
            g_content_width = 0;
        }
        g_scroll_y = 0;
        g_scroll_x = 0;

        HWND hContent = GetDlgItem(hwnd, ID_CONTENT);
        UpdateScrollBars(hContent);
        InvalidateRect(hContent, NULL, TRUE);

        HWND hHistory = GetDlgItem(hwnd, ID_HISTORY);
        if (hHistory) InvalidateRect(hHistory, NULL, TRUE);
    }
}

void Navigate(HWND hwnd, const char *url) {
    LOG_INFO("Navigating to: %s", url);

    HWND hUrlEdit = GetDlgItem(hwnd, ID_EDIT_URL);
    if (hUrlEdit) SetWindowText(hUrlEdit, url);

    // Center and show loading popup
    if (g_hLoading) {
        RECT rcMain;
        GetWindowRect(hwnd, &rcMain);
        int mainW = rcMain.right - rcMain.left;
        int mainH = rcMain.bottom - rcMain.top;
        int panelW = 300;
        int panelH = 150;
        int x = rcMain.left + (mainW - panelW) / 2;
        int y = rcMain.top + (mainH - panelH) / 2;
        
        SetWindowPos(g_hLoading, HWND_TOP, x, y, panelW, panelH, SWP_SHOWWINDOW);
        
        HWND hAnim = GetDlgItem(g_hLoading, ID_ANIM_CTRL);
        
        // Start Animation
        if (g_hShell32 && hAnim) {
            if (SendMessage(hAnim, ACM_OPEN, (WPARAM)g_hShell32, (LPARAM)MAKEINTRESOURCE(IDR_AVI_FILECOPY))) {
                SendMessage(hAnim, ACM_PLAY, (WPARAM)-1, MAKELPARAM(0, -1)); // Loop forever
            }
        }
        
        UpdateWindow(g_hLoading);
        UpdateWindow(hwnd);
        
        // Pump any pending messages to start animation
        MSG msg;
        while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    network_response_t *res = network_fetch(url);
    if (res && res->data) {
        ProcessResponse(hwnd, res, res->final_url ? res->final_url : url);
        network_response_free(res);
    } else {
        LOG_ERROR("Failed to fetch: %s", url);
    }

    if (g_hLoading) {
        HWND hAnim = GetDlgItem(g_hLoading, ID_ANIM_CTRL);
        if (hAnim) SendMessage(hAnim, ACM_STOP, 0, 0);
        ShowWindow(g_hLoading, SW_HIDE);
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
            // Render content
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
    return 0;
}

static LRESULT CALLBACK LoadingWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            RECT rc;
            GetClientRect(hwnd, &rc);
            // Draw a nice border
            FrameRect(hdc, &rc, GetSysColorBrush(COLOR_ACTIVECAPTION));
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
                g_manual_navigation = 1;
                Navigate(hwnd, url);
                g_manual_navigation = 0;
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

    if (hStar) MoveWindow(hStar, 0, topBarY, btnSize, btnSize, TRUE);
    if (hUrl) MoveWindow(hUrl, urlX, topBarY, urlWidth, btnSize, TRUE);
    if (hGo) MoveWindow(hGo, width - btnSize, topBarY, btnSize, btnSize, TRUE);

    // History pane on left
    int contentY = TOP_BAR_HEIGHT;
    int contentHeight = height - contentY;
    if (contentHeight < 0) contentHeight = 0;

    // Content area takes up the rest (right side)
    int contentX = HISTORY_WIDTH;
    int contentWidth = width - HISTORY_WIDTH;
    if (contentWidth < 0) contentWidth = 0;

    if (hHistory) MoveWindow(hHistory, 0, contentY, HISTORY_WIDTH, contentHeight, TRUE);
    if (hContent) MoveWindow(hContent, contentX, contentY, contentWidth, contentHeight, TRUE);

    // Update history_ui panel height
    history_ui_set_panel_height(contentHeight);
}
