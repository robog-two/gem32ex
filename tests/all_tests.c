#include <stdio.h>
#include "core/log.h"

// Forward declarations of test functions from other files
// We'll compile everything together
void run_network_tests(int *failed_count);
void run_core_tests(int *failed_count);

int main() {
    log_init();
    
    int total_failed = 0;
    
    printf("=======================================\n");
    printf("   Gem32 Integrated Test Suite\n");
    printf("=======================================\n");
    
    run_network_tests(&total_failed);
    run_core_tests(&total_failed);
    
    printf("\n=======================================\n");
    if (total_failed == 0) {
        printf("  GLOBAL STATUS: ALL TESTS PASSED\n");
    } else {
        printf("  GLOBAL STATUS: FAILED (%d total errors)\n", total_failed);
    }
    printf("=======================================\n");
    
    return total_failed;
}

