#include "log.h"
#include <windows.h>
#include <stdio.h>
#include <stdarg.h>
#include <io.h>
#include <fcntl.h>

static int g_has_console = 0;

void log_init(void) {
    // Try to attach to the parent's console
    if (AttachConsole(ATTACH_PARENT_PROCESS)) {
        g_has_console = 1;
    } else if (GetLastError() == ERROR_ACCESS_DENIED) {
        // Already attached to a console
        g_has_console = 1;
    }

    if (g_has_console) {
        // Redirect CRT standard streams to the console
        freopen("CONOUT$", "w", stdout);
        freopen("CONOUT$", "w", stderr);
        setvbuf(stdout, NULL, _IONBF, 0);
        setvbuf(stderr, NULL, _IONBF, 0);
    }
}

void log_msg(log_level_t level, const char* format, ...) {
    char buffer[1024];
    const char* level_str = "INFO";
    
    switch (level) {
        case LOG_LEVEL_DEBUG: level_str = "DEBUG"; break;
        case LOG_LEVEL_INFO:  level_str = "INFO "; break;
        case LOG_LEVEL_WARN:  level_str = "WARN "; break;
        case LOG_LEVEL_ERROR: level_str = "ERROR"; break;
    }

    va_list args;
    va_start(args, format);
    int len = vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);

    if (len > 0) {
        // Always output to debug stream
        char debug_buffer[1100];
        _snprintf(debug_buffer, sizeof(debug_buffer), "[%s] %s\n", level_str, buffer);
        OutputDebugString(debug_buffer);

        // Output to console if we have one
        if (g_has_console) {
            fprintf(stdout, "[%s] %s\n", level_str, buffer);
            fflush(stdout);
        }
    }
}