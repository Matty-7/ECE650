#include <stdio.h>
#include "my_malloc.h"

// double free
int main(){
    void* p = ff_malloc(100);
    ff_free(p);
    ff_free(p);
    printf("PASS: double free test (should at least not crash)\n");
    return 0;
}
