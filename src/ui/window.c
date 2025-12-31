#include "window.h"
#include <commctrl.h>
#include <stdio.h>
#include "network/http.h"
#include "core/html.h"
#include "core/style.h"
#include "core/layout.h"
#include "ui/history.h"
#include "ui/bookmarks.h"
#include "ui/render.h"

#define ID_BTN_STAR 101
#define ID_EDIT_URL 102
#define ID_BTN_GO   103
#define ID_CONTENT  104
#define ID_HISTORY  105

#define TOP_BAR_HEIGHT 30
#define HISTORY_HEIGHT 150

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
static LRESULT CALLBACK ContentWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
static void ResizeChildWindows(HWND hwnd, int width, int height);
static void Navigate(HWND hwnd, const char *url);

static history_tree_t *g_history = NULL;
static layout_box_t *g_current_layout = NULL;
static node_t *g_current_dom = NULL;

BOOL CreateMainWindow(HINSTANCE hInstance, int nCmdShow) {
    g_history = history_create();
    
    // Register Main Window Class
    const char className[] = "Gem32BrowserClass";
    WNDCLASS wc = {0};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = className;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);

    if (!RegisterClass(&wc)) return FALSE;

    // Register Content Window Class
    const char contentClassName[] = "Gem32ContentClass";
    WNDCLASS wcc = {0};
    wcc.lpfnWndProc = ContentWndProc;
    wcc.hInstance = hInstance;
    wcc.lpszClassName = contentClassName;
    wcc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wcc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1); // White background
    
    if (!RegisterClass(&wcc)) return FALSE;

    HWND hwnd = CreateWindowEx(
        0,
        className,
        "Gem32 Browser",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 800, 600,
        NULL, NULL, hInstance, NULL
    );

    if (hwnd == NULL) return FALSE;

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    return TRUE;
}

#include "network/loader.h"

// ...

static void Navigate(HWND hwnd, const char *url) {
    network_response_t *res = http_fetch(url);
    if (res && res->data) {
        // Free previous DOM and Layout
        if (g_current_layout) {
            layout_free(g_current_layout);
            g_current_layout = NULL;
        }
        if (g_current_dom) {
            node_free(g_current_dom);
            g_current_dom = NULL;
        }

        g_current_dom = html_parse(res->data);
        if (g_current_dom) {
            loader_fetch_resources(g_current_dom, url);
            style_compute(g_current_dom);
            
            HWND hContent = GetDlgItem(hwnd, ID_CONTENT);
            RECT rect;
            GetClientRect(hContent, &rect);
            int width = rect.right - rect.left;
            
            g_current_layout = layout_create_tree(g_current_dom, width);
            
            history_add(g_history, url, "Title Placeholder");
            
            // Trigger repaint
            InvalidateRect(hContent, NULL, TRUE);
        }
        network_response_free(res);
    } else {
        MessageBox(hwnd, "Failed to fetch URL", "Error", MB_ICONERROR);
    }
}

static LRESULT CALLBACK ContentWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            
            // Basic font setup
            HFONT hFont = GetStockObject(DEFAULT_GUI_FONT);
            SelectObject(hdc, hFont);

            if (g_current_layout) {
                render_tree(hdc, g_current_layout, 0, 0);
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
        case WM_CREATE:
            CreateWindow(
                "BUTTON", "*",
                WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
                0, 0, 0, 0,
                hwnd, (HMENU)ID_BTN_STAR,
                ((LPCREATESTRUCT)lParam)->hInstance, NULL
            );

            CreateWindow(
                "EDIT", "http://google.com",
                WS_VISIBLE | WS_CHILD | WS_BORDER | ES_AUTOHSCROLL,
                0, 0, 0, 0,
                hwnd, (HMENU)ID_EDIT_URL,
                ((LPCREATESTRUCT)lParam)->hInstance, NULL
            );

            CreateWindow(
                "BUTTON", "Go",
                WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,
                0, 0, 0, 0,
                hwnd, (HMENU)ID_BTN_GO,
                ((LPCREATESTRUCT)lParam)->hInstance, NULL
            );

            // Use our custom class for content
            CreateWindow(
                "Gem32ContentClass", NULL, 
                WS_VISIBLE | WS_CHILD | WS_BORDER | WS_VSCROLL | WS_HSCROLL, 
                0, 0, 0, 0,
                hwnd, (HMENU)ID_CONTENT,
                ((LPCREATESTRUCT)lParam)->hInstance, NULL
            );

            CreateWindow(
                "STATIC", "Git-Style History Tree",
                WS_VISIBLE | WS_CHILD | WS_BORDER | SS_CENTERIMAGE | SS_CENTER,
                0, 0, 0, 0,
                hwnd, (HMENU)ID_HISTORY,
                ((LPCREATESTRUCT)lParam)->hInstance, NULL
            );
            break;

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

    int contentY = TOP_BAR_HEIGHT;
    int historyY = height - HISTORY_HEIGHT;
    int contentHeight = historyY - contentY;

    if (contentHeight < 0) contentHeight = 0;

    if (hContent) MoveWindow(hContent, 0, contentY, width, contentHeight, TRUE);
    if (hHistory) MoveWindow(hHistory, 0, historyY, width, HISTORY_HEIGHT, TRUE);
}
