#include "window.h"
#include <commctrl.h>
#include <stdio.h>

#define ID_BTN_STAR 101
#define ID_EDIT_URL 102
#define ID_BTN_GO   103
#define ID_CONTENT  104
#define ID_HISTORY  105

#define TOP_BAR_HEIGHT 30
#define HISTORY_HEIGHT 150

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
static void ResizeChildWindows(HWND hwnd, int width, int height);

BOOL CreateMainWindow(HINSTANCE hInstance, int nCmdShow) {
    const char className[] = "Gem32BrowserClass";

    WNDCLASS wc = {0};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = className;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);

    if (!RegisterClass(&wc)) {
        MessageBox(NULL, "Window Registration Failed!", "Error", MB_ICONEXCLAMATION | MB_OK);
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
        MessageBox(NULL, "Window Creation Failed!", "Error", MB_ICONEXCLAMATION | MB_OK);
        return FALSE;
    }

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    return TRUE;
}

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE:
            // Top Bar: Star Button
            CreateWindow(
                "BUTTON", "*",
                WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
                0, 0, 0, 0, // Sized in WM_SIZE
                hwnd, (HMENU)ID_BTN_STAR,
                ((LPCREATESTRUCT)lParam)->hInstance, NULL
            );

            // Top Bar: URL Bar
            CreateWindow(
                "EDIT", "http://google.com",
                WS_VISIBLE | WS_CHILD | WS_BORDER | ES_AUTOHSCROLL,
                0, 0, 0, 0,
                hwnd, (HMENU)ID_EDIT_URL,
                ((LPCREATESTRUCT)lParam)->hInstance, NULL
            );

            // Top Bar: Go Button
            CreateWindow(
                "BUTTON", "Go",
                WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,
                0, 0, 0, 0,
                hwnd, (HMENU)ID_BTN_GO,
                ((LPCREATESTRUCT)lParam)->hInstance, NULL
            );

            // Content Area (White background static for now)
            CreateWindow(
                "STATIC", "Content Area",
                WS_VISIBLE | WS_CHILD | WS_BORDER | SS_CENTERIMAGE | SS_CENTER,
                0, 0, 0, 0,
                hwnd, (HMENU)ID_CONTENT,
                ((LPCREATESTRUCT)lParam)->hInstance, NULL
            );

            // History Tree Area (Gray background static for now)
            CreateWindow(
                "STATIC", "Git-Style History Tree",
                WS_VISIBLE | WS_CHILD | WS_BORDER | SS_CENTERIMAGE | SS_CENTER,
                0, 0, 0, 0,
                hwnd, (HMENU)ID_HISTORY,
                ((LPCREATESTRUCT)lParam)->hInstance, NULL
            );
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
    // Layout: [Star] [URL] [Go]
    
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
