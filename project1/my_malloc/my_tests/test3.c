#include <stdio.h>
#include "my_malloc.h"

// assign 3 blocks, free 2 blocks, check if they are merged

int main() {
    void *A = ff_malloc(64);
    void *B = ff_malloc(64);
    void *C = ff_malloc(64);

    ff_free(B);
    ff_free(A);   // A and B are merged
    ff_free(C);   // C should be merged with the large block

    unsigned long total = get_data_segment_size();
    unsigned long free_ = get_data_segment_free_space_size();

    // if coalesce is successful, free_ should be almost equal to total
    if (free_ + 16 < total) {
        printf("FAIL: coalesce not working properly.\n");
        return -1;
    }
    printf("PASS: coalesce test\n");
    return 0;
}
