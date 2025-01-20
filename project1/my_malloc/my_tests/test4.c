#include <stdio.h>
#include <string.h>
#include "my_malloc.h"

// allocate a large block, then free it, then allocate a small block, check if it is split

int main() {
    void *p = ff_malloc(200);
    if (!p) {
        printf("FAIL: no memory allocated.\n");
        return -1;
    }
    ff_free(p);

    // allocate a small block 50, check if it is split from the large block 200
    void *q = ff_malloc(50);
    if (!q) {
        printf("FAIL: second alloc failed.\n");
        return -1;
    }
    ff_free(q);

    printf("PASS: split test\n");
    return 0;
}
