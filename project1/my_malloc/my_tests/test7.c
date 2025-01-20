#include <stdio.h>
#include "my_malloc.h"

int main() {
    void* p = ff_malloc(0);
    if (p != NULL) {
        printf("FAIL: malloc(0) should return NULL or handle gracefully.\n");
        return -1;
    }
    // 测试极大分配(不一定能成功，只要不崩溃即可)
    void* big = ff_malloc(1024*1024*100);  // 100 MB
    if (big) {
        ff_free(big);
    }
    printf("PASS: zero + large alloc test\n");
    return 0;
}
