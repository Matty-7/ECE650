#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <sys/time.h> // I added this
#include "my_malloc.h"

#define NUM_ITERS    10000
#define NUM_ITEMS    10000
#define ALLOC_SIZE   128

#ifdef FF
#define MALLOC(sz) ff_malloc(sz)
#define FREE(p)    ff_free(p)
#endif
#ifdef BF
#define MALLOC(sz) bf_malloc(sz)
#define FREE(p)    bf_free(p)
#endif


double calc_time(struct timeval start, struct timeval end) {
  double start_sec = (double)start.tv_sec + (double)start.tv_usec / 1000000.0;
  double end_sec = (double)end.tv_sec + (double)end.tv_usec / 1000000.0;

  if (end_sec < start_sec) {
    return 0;
  } else {
    return end_sec - start_sec;
  }
};


int main(int argc, char *argv[])
{
  int i, j;
  int *array[NUM_ITEMS];
  int *spacing_array[NUM_ITEMS];
  unsigned long data_segment_size;
  unsigned long data_segment_free_space;
  struct timeval start_time, end_time;

  if (NUM_ITEMS < 10000) {
    printf("Error: NUM_ITEMS must be >= 1000\n");
    return -1;
  } //if

  for (i=0; i < NUM_ITEMS; i++) {
    array[i] = (int *)MALLOC(ALLOC_SIZE);
    spacing_array[i] = (int *)MALLOC(ALLOC_SIZE);
  } //for i

  for (i=0; i < NUM_ITEMS; i++) {
    FREE(array[i]);
  } //for i

  //Start Time
  gettimeofday(&start_time, NULL);

  for (i=0; i < NUM_ITERS; i++) {
    for (j=0; j < 1000; j++) {
      array[j] = (int *)MALLOC(ALLOC_SIZE);
    } //for j

    for (j=1000; j < NUM_ITEMS; j++) {
      array[j] = (int *)MALLOC(ALLOC_SIZE);
      FREE(array[j-1000]);

      if ((i==NUM_ITERS/2) && (j==NUM_ITEMS/2)) {
	//Record fragmentation halfway through (try to repsresent steady state)
	data_segment_size = get_data_segment_size();
	data_segment_free_space = get_data_segment_free_space_size();
      } //if
    } //for j

    for (j=NUM_ITEMS-1000; j < NUM_ITEMS; j++) {
      FREE(array[j]);
    } //for j
  } //for i

  //Stop Time
  gettimeofday(&end_time, NULL);

  double elapsed_sec = calc_time(start_time, end_time);
  printf("Execution Time = %f seconds\n", elapsed_sec);
  printf("Fragmentation  = %f\n", (float)data_segment_free_space/(float)data_segment_size);

  for (i=0; i < NUM_ITEMS; i++) {
    FREE(spacing_array[i]);
  }
  
  return 0;
}
