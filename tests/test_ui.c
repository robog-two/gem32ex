#include "test_ui.h"
#include "core/log.h"
#include <commctrl.h>
#include <stdio.h>

#define MAX_TESTS 64
static test_result_t g_results[MAX_TESTS];
static int g_test_count = 0;
static int g_passed_count = 0;

void run_test_case(const char *name, test_func_t func, int *total_failed) {
    printf("Testing %s... ", name);
    fflush(stdout);
    
    log_capture_start();
    int passed = func();
    char *logs = log_capture_stop();
    
    if (!passed) {
        printf("FAIL\n");
        (*total_failed)++;
    } else {
        printf("PASS\n");
    }
    fflush(stdout);
    
    test_ui_add_result(name, passed, logs);
}

void test_ui_add_result(const char *name, int passed, const char *logs) {
    if (g_test_count < MAX_TESTS) {
        g_results[g_test_count].name = name;
        g_results[g_test_count].passed = passed;
        g_results[g_test_count].logs = logs ? strdup(logs) : strdup("No logs.");
        if (passed) g_passed_count++;
        g_test_count++;
    }
}

static LRESULT CALLBACK TestWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_DESTROY:
            PostQuitMessage(0);
            break;
        case WM_SIZE: {
            int w = LOWORD(lParam);
            int h = HIWORD(lParam);
            HWND hStatus = GetDlgItem(hwnd, 2);
            HWND hProgress = GetDlgItem(hwnd, 3);
            HWND hTree = GetDlgItem(hwnd, 1);
            
            if (hStatus) MoveWindow(hStatus, 10, 10, w - 20, 30, TRUE);
            if (hProgress) MoveWindow(hProgress, 10, 45, w - 20, 20, TRUE);
            if (hTree) MoveWindow(hTree, 10, 75, w - 20, h - 85, TRUE);
            break;
        }
        default:
            return DefWindowProc(hwnd, msg, wParam, lParam);
    }
    return 0;
}

void test_ui_show(void) {
    HINSTANCE hInstance = GetModuleHandle(NULL);
    InitCommonControls();

    WNDCLASS wc = {0};
    wc.lpfnWndProc = TestWndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = "Gem32TestUI";
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    RegisterClass(&wc);

    HWND hwnd = CreateWindow("Gem32TestUI", "Gem32 Integrated Test Suite", 
                             WS_OVERLAPPEDWINDOW | WS_VISIBLE,
                             CW_USEDEFAULT, CW_USEDEFAULT, 700, 500,
                             NULL, NULL, hInstance, NULL);

    // Summary Text
    char summary[128];
    sprintf(summary, "Total Tests: %d | Passed: %d | Failed: %d", g_test_count, g_passed_count, g_test_count - g_passed_count);
    HWND hStatus = CreateWindow("STATIC", summary, WS_VISIBLE | WS_CHILD | SS_CENTERIMAGE, 
                               10, 10, 680, 30, hwnd, (HMENU)2, hInstance, NULL);
    
    // Set a nicer font for summary
    SendMessage(hStatus, WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT), TRUE);

    // Progress Bar
    HWND hProgress = CreateWindow(PROGRESS_CLASS, NULL, WS_VISIBLE | WS_CHILD | PBS_SMOOTH,
                                 10, 45, 680, 20, hwnd, (HMENU)3, hInstance, NULL);
    SendMessage(hProgress, PBM_SETRANGE, 0, MAKELPARAM(0, g_test_count));
    SendMessage(hProgress, PBM_SETPOS, g_passed_count, 0);

    // TreeView
    HWND hTree = CreateWindowEx(WS_EX_CLIENTEDGE, WC_TREEVIEW, "", 
                               WS_VISIBLE | WS_CHILD | TVS_HASBUTTONS | TVS_HASLINES | TVS_LINESATROOT,
                               10, 75, 680, 380, hwnd, (HMENU)1, hInstance, NULL);

    HIMAGELIST hImg = ImageList_Create(16, 16, ILC_COLOR32 | ILC_MASK, 2, 2);
    ImageList_AddIcon(hImg, LoadIcon(NULL, IDI_ASTERISK)); // Pass
    ImageList_AddIcon(hImg, LoadIcon(NULL, IDI_HAND));     // Fail (X)
    TreeView_SetImageList(hTree, hImg, TVSIL_NORMAL);

    HFONT hFontMono = CreateFont(-12, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                 ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                 DEFAULT_QUALITY, FIXED_PITCH | FF_MODERN, "Courier New");

    for (int i = 0; i < g_test_count; i++) {
        TVINSERTSTRUCT tvi = {0};
        tvi.hParent = TVI_ROOT;
        tvi.hInsertAfter = TVI_LAST;
        tvi.item.mask = TVIF_TEXT | TVIF_IMAGE | TVIF_SELECTEDIMAGE | TVIF_STATE;
        tvi.item.pszText = (char*)g_results[i].name;
        tvi.item.iImage = g_results[i].passed ? 0 : 1;
        tvi.item.iSelectedImage = tvi.item.iImage;
        
        // Highlight failure with bold state if supported or just the icon
        if (!g_results[i].passed) {
            tvi.item.state = TVIS_BOLD;
            tvi.item.stateMask = TVIS_BOLD;
        }

        HTREEITEM hItem = TreeView_InsertItem(hTree, &tvi);

        // Add logs as child
        TVINSERTSTRUCT tvi_log = {0};
        tvi_log.hParent = hItem;
        tvi_log.hInsertAfter = TVI_LAST;
        tvi_log.item.mask = TVIF_TEXT;
        tvi_log.item.pszText = g_results[i].logs;
        TreeView_InsertItem(hTree, &tvi_log);
    }

    // Force size message to layout children
    RECT rect;
    GetClientRect(hwnd, &rect);
    SendMessage(hwnd, WM_SIZE, 0, MAKELPARAM(rect.right, rect.bottom));

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    
    DeleteObject(hFontMono);
}