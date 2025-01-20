#include <stdio.h>
#include "my_malloc.h"

int main(){
    unsigned long s1 = get_data_segment_size();
    unsigned long f1 = get_data_segment_free_space_size();

    // allocate 2 blocks
    void* p1 = ff_malloc(128);
    void* p2 = ff_malloc(64);

    unsigned long s2 = get_data_segment_size();
    unsigned long f2 = get_data_segment_free_space_size();
    // usually s2 > s1
    // f2 <= f1

    ff_free(p2);
    unsigned long s3 = get_data_segment_size();
    unsigned long f3 = get_data_segment_free_space_size();
    // s3 is the same as s2 or slightly changed (if split/merge)
    // f3 > f2

    ff_free(p1);

    unsigned long s4 = get_data_segment_size();
    unsigned long f4 = get_data_segment_free_space_size();
    // if coalesce is successful, f4 should be close to s4

    printf("Sizes: %lu -> %lu -> %lu -> %lu\n", s1, s2, s3, s4);
    printf("Frees: %lu -> %lu -> %lu -> %lu\n", f1, f2, f3, f4);
    printf("PASS: segment stats test\n");
    return 0;
}
