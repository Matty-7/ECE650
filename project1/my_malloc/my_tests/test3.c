#include <stdio.h>
#include "my_malloc.h"

// 分配 A, B, C，释放 B，再释放 A，看是否与 B 合并，再释放 C，检查最后能否合并成一个 free 块

int main() {
    void *A = ff_malloc(64);
    void *B = ff_malloc(64);
    void *C = ff_malloc(64);

    ff_free(B);
    ff_free(A);   // A 和 B 应该合并
    ff_free(C);   // C 应该继续与上面的大块再合并

    unsigned long total = get_data_segment_size();
    unsigned long free_ = get_data_segment_free_space_size();

    // 若都合并成功，那么 free_ 应该几乎等于 total
    if (free_ + 16 < total) {
        printf("FAIL: coalesce not working properly.\n");
        return -1;
    }
    printf("PASS: coalesce test\n");
    return 0;
}
