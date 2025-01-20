#include <stdio.h>
#include <stdlib.h>
#include "my_malloc.h"

// multiple malloc/free cycles
int main() {
    for (int round=0; round<5; round++) {
        void* arr[10];
        for(int i=0; i<10; i++){
            arr[i] = ff_malloc(100 + i*10);
        }
        for(int i=0; i<10; i++){
            ff_free(arr[i]);
        }
    }
    printf("PASS: repeated alloc/free cycles\n");
    return 0;
}
