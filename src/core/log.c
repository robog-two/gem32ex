#include "log.h"
#include <windows.h>
#include <stdio.h>
#include <stdarg.h>

void log_init(void) {
    // Just ensure output is unbuffered so it shows up immediately in the console
    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);
}

void log_msg(log_level_t level, const char* format, ...) {
    char buffer[1024];
    const char* level_str = "INFO";
    FILE* out = stdout;
    
    switch (level) {
        case LOG_LEVEL_DEBUG: level_str = "DEBUG"; break;
        case LOG_LEVEL_INFO:  level_str = "INFO "; break;
        case LOG_LEVEL_WARN:  level_str = "WARN "; out = stderr; break;
        case LOG_LEVEL_ERROR: level_str = "ERROR"; out = stderr; break;
    }

    va_list args;
    va_start(args, format);
    int len = vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);

    if (len > 0) {
        // Print to the actual standard streams
        fprintf(out, "[%s] %s\n", level_str, buffer);
        
        // Also always output to debug stream (for DebugView/IDE)
        char debug_buffer[1100];
        _snprintf(debug_buffer, sizeof(debug_buffer), "[%s] %s\n", level_str, buffer);
        OutputDebugString(debug_buffer);
    }
}
