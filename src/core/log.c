#include "log.h"
#include <windows.h>
#include <stdio.h>
#include <stdarg.h>

static HANDLE hConsole = NULL;

void log_init(void) {
    if (AllocConsole()) {
        hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
        // Redirect standard streams to the new console
        freopen("CONOUT$", "w", stdout);
        freopen("CONOUT$", "w", stderr);
        
        // Also set console title
        SetConsoleTitle("Gem32 Debug Console");
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
        // Output to console if available
        if (hConsole) {
            printf("[%s] %s\n", level_str, buffer);
            fflush(stdout);
        }
        
        // Always output to debug stream
        char debug_buffer[1100];
        _snprintf(debug_buffer, sizeof(debug_buffer), "[%s] %s\n", level_str, buffer);
        OutputDebugString(debug_buffer);
    }
}
