#include <stdlib.h>
#include <stdio.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <semaphore.h>
#include <unistd.h>
#include <sys/wait.h>

// have a parent and child cooperate on counting to 200 (each doing 100)
// this version grabs the mutex inside the counting loop, so results are mixed

#define SEM_NAME  "my_sem"

int main(int argc, char *argv[])
{
  int fd, i;
  int *ptr;
  int init_value = 0;
  
  sem_unlink(SEM_NAME); // destroy it if it already exists from a past failed run
  
  //Initialize the semaphore (which will serve as a mutex)
  sem_t *mutex = sem_open(SEM_NAME, O_CREAT, 0644, 1);

  //Open a file that we will later map
  fd = open("mmapped_data.dat", O_RDWR | O_CREAT, 0644);

  //Write an initial value into the start of the file
  write(fd, &init_value, sizeof(int));

  //Map the file into this process
  ptr = mmap(NULL, sizeof(int), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  close(fd);

  //Child process(es) inherit memory mapped address space regions
  int id = fork();

  if (id == 0) {
    for (i=0; i < 100; i++) {
      sem_wait(mutex);
      printf("child: %d\n", (*ptr)++);
      sem_post(mutex);
    }
  } else {
    for (i=0; i < 100; i++) {
      sem_wait(mutex);
      printf("parent: %d\n", (*ptr)++);
      sem_post(mutex);
    }

    wait(NULL); //Wait for child process to complete
  }
  
  msync(ptr, sizeof(int), MS_SYNC); // force a flush of the shared memory to the underlying file (unnecessary for using shared memory, but useful in demo to see result in file)

  return 0;
}
