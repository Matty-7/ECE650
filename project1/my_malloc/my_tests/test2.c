#include <stdio.h>
#include <string.h>
#include "my_malloc.h"

int main() {
    void *p1 = ff_malloc(50), *p2 = ff_malloc(200), *p3 = ff_malloc(80);
    if (!p1 || !p2 || !p3) {
        printf("FAIL: Some malloc returned NULL\n");
        return -1;
    }
    memset(p2, 0xAB, 200);
    ff_free(p1);
    ff_free(p2);
    ff_free(p3);
    printf("PASS: multiple block malloc/free\n");
    return 0;
}
