#include <stdio.h>
#include "my_malloc.h"

int main() {
    void* p1 = ff_malloc(16);
    void* p2 = ff_malloc(128);
    void* p3 = ff_malloc(512);
    ff_free(p2);
    void* p4 = ff_malloc(64);  // try to reuse the space of p2
    ff_free(p1);
    ff_free(p3);
    ff_free(p4);
    printf("PASS: mixed size test\n");
    return 0;
}
