#include "ui/window.h"
#include "core/log.h"

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    (void)hPrevInstance;
    (void)lpCmdLine;

    log_init();
    LOG_INFO("Application starting...");

    if (!CreateMainWindow(hInstance, nCmdShow)) {
        LOG_ERROR("Failed to create main window");
        return 1;
    }

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return msg.wParam;
}
