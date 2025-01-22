#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <assert.h>
#include "my_malloc.h"

// 如果想测试 FF 版本，就用编译指令: gcc -DFF test_my_malloc.c my_malloc.c -o test_my_malloc
// 如果想测试 BF 版本，就用: gcc -DBF test_my_malloc.c my_malloc.c -o test_my_malloc

#ifdef FF
    #define MALLOC ff_malloc
    #define FREE   ff_free
    const char* ALLOC_MODE = "First-Fit";
#elif BF
    #define MALLOC bf_malloc
    #define FREE   bf_free
    const char* ALLOC_MODE = "Best-Fit";
#else
    // 默认用 first-fit
    #define MALLOC ff_malloc
    #define FREE   ff_free
    const char* ALLOC_MODE = "First-Fit (default)";
#endif

static void print_segment_info(const char* msg) {
    unsigned long total_size = get_data_segment_size();
    unsigned long free_size  = get_data_segment_free_space_size();
    printf("%s: data_segment_size = %lu, free_space = %lu, fragmentation = %.3f\n",
           msg, total_size, free_size,
           (total_size == 0) ? 0.0 : (double)free_size / (double)total_size);
}

// ========== Test 1: Basic allocate/free correctness ==========
void test_basic() {
    printf("===== Test Basic Alloc/Free =====\n");
    print_segment_info("Initial");

    void* ptr1 = MALLOC(100);
    assert(ptr1 != NULL);
    memset(ptr1, 0xAB, 100);

    void* ptr2 = MALLOC(200);
    assert(ptr2 != NULL);
    memset(ptr2, 0xCD, 200);

    print_segment_info("After 2 allocations");

    FREE(ptr1);
    print_segment_info("After free(ptr1)");

    FREE(ptr2);
    print_segment_info("After free(ptr2)");

    // 如果全部释放后合并良好，那么 free_space 应该接近 total_size
    printf("===== End of Test Basic =====\n\n");
}

// ========== Test 2: Coalescing check (连续 free) ==========
void test_coalesce() {
    printf("===== Test Coalescing =====\n");
    print_segment_info("Initial");

    // 申请 5 块连续的小块
    const int N = 5;
    void* blocks[N];
    for(int i=0; i<N; i++){
        blocks[i] = MALLOC(128);
        assert(blocks[i] != NULL);
        memset(blocks[i], i, 128);
    }
    print_segment_info("After 5 consecutive allocations");

    // 全部按顺序 free，如果合并逻辑正确，应只剩一个大空闲块
    for(int i=0; i<N; i++){
        FREE(blocks[i]);
    }
    print_segment_info("After freeing all blocks (should coalesce)");

    printf("===== End of Test Coalescing =====\n\n");
}

// ========== Test 3: Splitting check ==========
void test_split() {
    printf("===== Test Splitting =====\n");
    print_segment_info("Initial");

    // 申请一个块，再次申请一个更小块，看是否 split
    void* big = MALLOC(1024);
    // big 块本身不会立即 split，但可能影响后续行为
    memset(big, 0xA1, 1024);

    // 释放 big，变成空闲块
    FREE(big);

    // 现在再申请一个比 1024 小很多的块，比如 100
    // 正确做法：会从这块 1024 字节的空闲中 split 出 100 和剩余空间
    void* small = MALLOC(100);
    memset(small, 0xB2, 100);

    print_segment_info("After split (1024 -> 100 + remainder)");

    FREE(small);
    print_segment_info("After freeing small block");

    // 如果分裂和合并都正确，这里应该又 coalesce 回一个 1024 左右的空闲块

    printf("===== End of Test Splitting =====\n\n");
}

// ========== Test 4: Random small allocations ==========
void test_random_small(int num_ops) {
    printf("===== Test Random Small =====\n");
    print_segment_info("Initial");

    srand(0x12345);

    // 为了可重复，我们先保留所有指针和大小
    void** ptrs = calloc(num_ops, sizeof(void*));
    size_t* sizes = calloc(num_ops, sizeof(size_t));

    // 先分配
    for(int i=0; i<num_ops; i++){
        sizes[i] = (rand() % 200) + 8; // 8~207字节
        ptrs[i] = MALLOC(sizes[i]);
        assert(ptrs[i] != NULL);
        // 写一些字节，防止优化掉
        memset(ptrs[i], i & 0xFF, sizes[i]);
        if(i % (num_ops/10) == 0) {
            print_segment_info("Progress");
        }
    }

    print_segment_info("After random small allocations");

    // 再随机释放一半
    for(int i=0; i<num_ops; i++){
        if(rand() % 2 == 0) {
            FREE(ptrs[i]);
            ptrs[i] = NULL;
        }
    }
    print_segment_info("After freeing half of them");

    // 再分配同样数量的一半
    for(int i=0; i<num_ops; i++){
        if(ptrs[i] == NULL) {
            size_t new_size = (rand() % 200) + 16;
            ptrs[i] = MALLOC(new_size);
            if(ptrs[i]) memset(ptrs[i], (i * 2) & 0xFF, new_size);
        }
    }
    print_segment_info("After second wave of allocations");

    // 释放全部
    for(int i=0; i<num_ops; i++){
        if(ptrs[i]) {
            FREE(ptrs[i]);
        }
    }
    print_segment_info("After freeing all");

    free(ptrs);
    free(sizes);

    printf("===== End of Test Random Small =====\n\n");
}

// ========== Test 5: Random large allocations ==========
void test_random_large(int num_ops) {
    printf("===== Test Random Large =====\n");
    srand(0x54321);

    // 此测试模拟 large range rand allocs
    // 先大量随机分配，可能高达几千~几万字节块

    const size_t MAX_SIZE = 64 * 1024; // 64KB
    void** ptrs = calloc(num_ops, sizeof(void*));
    size_t* sizes = calloc(num_ops, sizeof(size_t));

    for(int i=0; i<num_ops; i++){
        sizes[i] = (rand() % (MAX_SIZE - 512)) + 512; 
        ptrs[i] = MALLOC(sizes[i]);
        if(!ptrs[i]) {
            // 如果 sbrk() 失败，可能返回 NULL
            fprintf(stderr, "Allocation failed at i=%d, size=%zu\n", i, sizes[i]);
            break;
        }
        memset(ptrs[i], (i & 0xFF), sizes[i]);
    }
    print_segment_info("After random large allocations");

    // 再全部释放
    for(int i=0; i<num_ops; i++){
        if(ptrs[i]) {
            FREE(ptrs[i]);
        }
    }
    print_segment_info("After freeing all large blocks");

    free(ptrs);
    free(sizes);

    printf("===== End of Test Random Large =====\n\n");
}


// ========== Main ==========
int main(int argc, char** argv) {
    printf("Running custom malloc tests with %s.\n", ALLOC_MODE);

    // 1) 基础分配测试
    test_basic();

    // 2) 连续分配再释放，考查 coalescing
    test_coalesce();

    // 3) 分裂测试
    test_split();

    // 4) 适度随机的小分配测试
    test_random_small(5000);

    // 5) 大范围随机分配测试
    //    (如果你的内存不够，可以调小这里的次数或者 MAX_SIZE)
    test_random_large(2000);

    printf("All tests done.\n");
    return 0;
}
