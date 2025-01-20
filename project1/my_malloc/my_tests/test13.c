#include <stdio.h>
#include <stdlib.h>
#include "my_malloc.h"

static void print_stats(const char* msg) {
    unsigned long total = get_data_segment_size();
    unsigned long free_ = get_data_segment_free_space_size();
    printf("[%s] total=%lu, free=%lu, used=%ld\n", 
           msg, total, free_, (long)(total - free_));
}

int main() {
    printf("=== DEBUG TEST (test13) ===\n");

    print_stats("start");

    void* X = ff_malloc(32);
    printf("Allocated X=32 at %p\n", X);
    print_stats("after X alloc");

    void* Y = ff_malloc(64);
    printf("Allocated Y=64 at %p\n", Y);
    print_stats("after Y alloc");

    ff_free(X);
    printf("Freed X=32 at %p\n", X);
    print_stats("after X free");

    // Another allocation that may reuse X
    void* W = ff_malloc(32);
    printf("Allocated W=32? at %p\n", W);
    print_stats("after W alloc");

    void* Z = ff_malloc(128);
    printf("Allocated Z=128 at %p\n", Z);
    print_stats("after Z alloc");

    // Free everything in a different order
    ff_free(Y);
    printf("Freed Y=64 at %p\n", Y);
    print_stats("after Y free");

    ff_free(Z);
    printf("Freed Z=128 at %p\n", Z);
    print_stats("after Z free");

    ff_free(W);
    printf("Freed W=32 at %p\n", W);
    print_stats("after W free");

    // If everything merges, free_ ~ total
    unsigned long final_total = get_data_segment_size();
    unsigned long final_free_ = get_data_segment_free_space_size();
    if (final_free_ + 16 < final_total) {
        printf("FAIL: coalesce not working properly.\n");
    } else {
        printf("PASS: coalescing check\n");
    }
    return 0;
}
