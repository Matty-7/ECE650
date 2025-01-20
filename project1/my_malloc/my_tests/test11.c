#include <stdio.h>
#include "my_malloc.h"

int main(){
    void* p = ff_malloc(100);
    ff_free(p);
    ff_free(p);  // 虽然题目说不会有这种情况，但看你是否会崩溃
    printf("PASS: double free test (should at least not crash)\n");
    return 0;
}
