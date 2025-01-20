#include <stdio.h>
#include <stdlib.h>
#include "my_malloc.h"

// random small allocs/frees
int main() {
    srand(0);
    void* arr[100];
    for (int i=0; i<100; i++){
        size_t sz = (rand() % 200) + 50;
        arr[i] = ff_malloc(sz);
    }
    for (int i=0; i<100; i++){
        if (arr[i]) ff_free(arr[i]);
    }
    printf("PASS: random small allocs/frees\n");
    return 0;
}
