#include "log.h"
#include <windows.h>
#include <stdio.h>
#include <stdarg.h>

void log_init(void) {
    // Attempt to attach to parent console (if any)
    if (AttachConsole(ATTACH_PARENT_PROCESS)) {
        // Redirect standard streams to the attached console
        freopen("CONOUT$", "w", stdout);
        freopen("CONOUT$", "w", stderr);
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
        // Output to stdout (might be redirected or attached console)
        printf("[%s] %s\n", level_str, buffer);
        fflush(stdout);
        
        // Always output to debug stream
        char debug_buffer[1100];
        _snprintf(debug_buffer, sizeof(debug_buffer), "[%s] %s\n", level_str, buffer);
        OutputDebugString(debug_buffer);
    }
}