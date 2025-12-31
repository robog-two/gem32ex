#ifndef LOG_H
#define LOG_H

typedef enum {
    LOG_LEVEL_DEBUG,
    LOG_LEVEL_INFO,
    LOG_LEVEL_WARN,
    LOG_LEVEL_ERROR
} log_level_t;

void log_init(void);
void log_msg(log_level_t level, const char* format, ...);

void log_capture_start(void);
char* log_capture_stop(void);

#define LOG_DEBUG(...) log_msg(LOG_LEVEL_DEBUG, __VA_ARGS__)
#define LOG_INFO(...)  log_msg(LOG_LEVEL_INFO, __VA_ARGS__)
#define LOG_WARN(...)  log_msg(LOG_LEVEL_WARN, __VA_ARGS__)
#define LOG_ERROR(...) log_msg(LOG_LEVEL_ERROR, __VA_ARGS__)

#endif
