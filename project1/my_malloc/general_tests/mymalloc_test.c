#include <stdlib.h>
#include <stdio.h>
#include "my_malloc.h"

#ifdef FF
#define MALLOC(sz) ff_malloc(sz)
#define FREE(p)    ff_free(p)
#endif
#ifdef BF
#define MALLOC(sz) bf_malloc(sz)
#define FREE(p)    bf_free(p)
#endif


int main(int argc, char *argv[])
{
  const unsigned NUM_ITEMS = 10;
  int i;
  int size;
  int sum = 0;
  int expected_sum = 0;
  int *array[NUM_ITEMS];

  size = 4;
  expected_sum += size * size;
  array[0] = (int *)MALLOC(size * sizeof(int));
  for (i=0; i < size; i++) {
    array[0][i] = size;
  } //for i
  for (i=0; i < size; i++) {
    sum += array[0][i];
  } //for i

  size = 16;
  expected_sum += size * size;
  array[1] = (int *)MALLOC(size * sizeof(int));
  for (i=0; i < size; i++) {
    array[1][i] = size;
  } //for i
  for (i=0; i < size; i++) {
    sum += array[1][i];
  } //for i

  size = 8;
  expected_sum += size * size;
  array[2] = (int *)MALLOC(size * sizeof(int));
  for (i=0; i < size; i++) {
    array[2][i] = size;
  } //for i
  for (i=0; i < size; i++) {
    sum += array[2][i];
  } //for i

  size = 32;
  expected_sum += size * size;
  array[3] = (int *)MALLOC(size * sizeof(int));
  for (i=0; i < size; i++) {
    array[3][i] = size;
  } //for i
  for (i=0; i < size; i++) {
    sum += array[3][i];
  } //for i

  FREE(array[0]);
  FREE(array[2]);

  size = 7;
  expected_sum += size * size;
  array[4] = (int *)MALLOC(size * sizeof(int));
  for (i=0; i < size; i++) {
    array[4][i] = size;
  } //for i
  for (i=0; i < size; i++) {
    sum += array[4][i];
  } //for i

  size = 256;
  expected_sum += size * size;
  array[5] = (int *)MALLOC(size * sizeof(int));
  for (i=0; i < size; i++) {
    array[5][i] = size;
  } //for i
  for (i=0; i < size; i++) {
    sum += array[5][i];
  } //for i

  FREE(array[5]);
  FREE(array[1]);
  FREE(array[3]);

  size = 23;
  expected_sum += size * size;
  array[6] = (int *)MALLOC(size * sizeof(int));
  for (i=0; i < size; i++) {
    array[6][i] = size;
  } //for i
  for (i=0; i < size; i++) {
    sum += array[6][i];
  } //for i

  size = 4;
  expected_sum += size * size;
  array[7] = (int *)MALLOC(size * sizeof(int));
  for (i=0; i < size; i++) {
    array[7][i] = size;
  } //for i
  for (i=0; i < size; i++) {
    sum += array[7][i];
  } //for i

  FREE(array[4]);

  size = 10;
  expected_sum += size * size;
  array[8] = (int *)MALLOC(size * sizeof(int));
  for (i=0; i < size; i++) {
    array[8][i] = size;
  } //for i
  for (i=0; i < size; i++) {
    sum += array[8][i];
  } //for i

  size = 32;
  expected_sum += size * size;
  array[9] = (int *)MALLOC(size * sizeof(int));
  for (i=0; i < size; i++) {
    array[9][i] = size;
  } //for i
  for (i=0; i < size; i++) {
    sum += array[9][i];
  } //for i

  FREE(array[6]);
  FREE(array[7]);
  FREE(array[8]);
  FREE(array[9]);

  if (sum == expected_sum) {
    printf("Calculated expected value of %d\n", sum);
    printf("Test passed\n");
  } else {
    printf("Expected sum=%d but calculated %d\n", expected_sum, sum);
    printf("Test failed\n");
  } //else

  // -------------- 新增测试 --------------
  // 1) 测试零字节分配（一般应返回NULL或不分配）
  printf("\n[Additional Test 1] Zero-Size Allocation\n");
  void* zero_ptr = MALLOC(0);
  if (zero_ptr == NULL) {
    printf("Zero-size allocation returned NULL as expected.\n");
  } else {
    printf("Zero-size allocation did NOT return NULL. Potential issue.\n");
    FREE(zero_ptr); // 小心释放
  }

  // 2) 多次分配并释放相同大小，以测试内部碎片与合并
  printf("\n[Additional Test 2] Repeated Alloc/Free of same size\n");
  int repeat_count = 20;
  size = 64;
  int* test_array = NULL;
  for (i = 0; i < repeat_count; i++) {
    test_array = (int*)MALLOC(size * sizeof(int));
    if (!test_array) {
      printf("Allocation failed at iteration %d\n", i);
      break;
    }
    // 写入一点数据，用于测试
    test_array[0] = 12345;
    FREE(test_array);
  }
  printf("Repeated %d times alloc/free for size=%d * sizeof(int)\n", repeat_count, size);

  // 3) 混合分配不同大小，看是否能正常处理
  printf("\n[Additional Test 3] Mixed allocations and partial frees\n");
  #define NUM_MIX 5
  int* mix_arr[NUM_MIX];
  const int mix_sizes[] = {5, 200, 128, 5, 64};
  for (i = 0; i < NUM_MIX; i++) {
    mix_arr[i] = (int*)MALLOC(mix_sizes[i] * sizeof(int));
    if (!mix_arr[i]) {
      printf("Mixed alloc %d failed\n", i);
      break;
    }
    // 写点数据
    mix_arr[i][0] = mix_sizes[i];
  }
  // 释放中间的三个分配
  for (i = 1; i < 4; i++) {
    FREE(mix_arr[i]);
  }
  // 再分配一个更大的块，看是否能找到合适的空闲空间
  int *big_block = (int*)MALLOC(300 * sizeof(int));
  if (big_block) {
    big_block[0] = 9999; 
    printf("Big block allocated successfully after partial frees.\n");
    FREE(big_block);
  } else {
    printf("Big block allocation failed after partial frees.\n");
  }
  // 最后释放剩余
  FREE(mix_arr[0]);
  FREE(mix_arr[4]);

  printf("\n[Additional Test 3] Completed.\n");

  return 0;
}
