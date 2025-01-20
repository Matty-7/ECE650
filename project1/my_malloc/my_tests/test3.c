#include <stdio.h>
#include "my_malloc.h"

// assign 3 blocks, free 2 blocks, check if they are merged

static void print_stats(const char* msg) {
    unsigned long total = get_data_segment_size();
    unsigned long free_ = get_data_segment_free_space_size();
    printf("[%s] total=%lu, free=%lu, used=%ld\n", 
           msg, total, free_, (long)(total - free_));
}

int main() {
    printf("=== DEBUG VERSION OF TEST3 ===\n");

    print_stats("start");

    void* A = ff_malloc(64);
    printf("Allocated A=64 at %p\n", A);
    print_stats("after A alloc");

    void* B = ff_malloc(64);
    printf("Allocated B=64 at %p\n", B);
    print_stats("after B alloc");

    void* C = ff_malloc(64);
    printf("Allocated C=64 at %p\n", C);
    print_stats("after C alloc");

    // Free B
    ff_free(B);
    printf("Freed B at %p\n", B);
    print_stats("after B free");

    // Free A; should coalesce with B 
    ff_free(A);
    printf("Freed A at %p -> A+B merged?\n", A);
    print_stats("after A free");

    // Free C; should merge with the A+B chunk if physically contiguous
    ff_free(C);
    printf("Freed C at %p -> now coalesce with big block?\n", C);
    print_stats("after C free");

    // Final check: free_ should be nearly total
    unsigned long final_total = get_data_segment_size();
    unsigned long final_free_ = get_data_segment_free_space_size();
    if (final_free_ + 16 < final_total) {
        printf("FAIL: coalesce not working properly.\n");
        return -1;
    }
    printf("PASS: coalesce test\n");
    return 0;
}
