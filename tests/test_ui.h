#ifndef TEST_UI_H
#define TEST_UI_H

#include <windows.h>

typedef struct {
    const char *name;
    int passed;
    char *logs;
} test_result_t;

void test_ui_add_result(const char *name, int passed, const char *logs);
void test_ui_show(void);

// Helper to run a test function with automatic log capture and UI reporting
typedef int (*test_func_t)(void);
void run_test_case(const char *name, test_func_t func, int *total_failed);

#endif