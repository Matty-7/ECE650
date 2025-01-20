#include <stdio.h>
#include "my_malloc.h"

// repeated same-size alloc/free
int main(){
    for(int i=0; i<1000; i++){
        void* p = ff_malloc(128);
        ff_free(p);
    }
    // 如果没有内存泄漏或异常
    printf("PASS: repeated same-size alloc/free\n");
    return 0;
}
