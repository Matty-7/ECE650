#include <stdio.h>
#include "my_malloc.h"

// single malloc/free
int main() {
    void* p = ff_malloc(100);
    if (!p) {
        printf("FAIL: malloc(100) returned NULL\n");
        return -1;
    }
    ff_free(p);
    printf("PASS: single malloc/free\n");
    return 0;
}
