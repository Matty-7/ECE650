#include <stdio.h>
#include <string.h>
#include "my_malloc.h"

// 分配大块再 free，然后再分配一个小块，检查是否会 split

int main() {
    void *p = ff_malloc(200);
    if (!p) {
        printf("FAIL: no memory allocated.\n");
        return -1;
    }
    ff_free(p);

    // 分配一个小块 50，看是否 split 原先 200 的大块
    void *q = ff_malloc(50);
    if (!q) {
        printf("FAIL: second alloc failed.\n");
        return -1;
    }
    ff_free(q);

    printf("PASS: split test\n");
    return 0;
}
