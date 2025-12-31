#include <stdio.h>
#include <windows.h>
#include "core/log.h"
#include "test_ui.h"

// Refactor tests to return status and maybe logs
// For now, we'll adapt them to use the UI
void run_network_tests(int *failed_count);
void run_core_tests(int *failed_count);

// We need a way to capture results into the UI
// In a real scenario, we'd pass a callback or use the UI functions directly inside tests

int main() {
    setvbuf(stdout, NULL, _IONBF, 0);
    
    int total_failed = 0;
    
    printf("=======================================\n");
    printf("   Gem32 Integrated Test Suite\n");
    printf("=======================================\n");
    
    // We'll modify the run_* functions slightly to register results in the UI if we want it fully integrated
    // For now, let's just run them and then show UI based on what we can gather.
    // Actually, let's modify the tests to call test_ui_add_result.
    
    run_network_tests(&total_failed);
    run_core_tests(&total_failed);
    
    printf("\nLaunching UI results viewer...\n");
    test_ui_show();
    
    return total_failed;
}