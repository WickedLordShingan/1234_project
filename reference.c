#include <fcntl.h>   /* For O_* constants */
#include <pthread.h> /* For pthread_mutex_* */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h> /* For shm_open, mmap */
#include <sys/wait.h> /* For wait() */
#include <unistd.h>   /* For ftruncate, fork, sleep */

#define SHM_NAME "/my_shared_memory"
#define ITERATIONS 5

/* Structure that will be placed in shared memory */
typedef struct {
  pthread_mutex_t mutex;
  int counter;
} SharedData;

int main() {
  int shm_fd;
  SharedData *shared_data;
  pthread_mutexattr_t mutex_attr;

  // ==========================================
  // 1. INITIALIZATION STAGE
  // ==========================================

  // Create shared memory object
  shm_fd = shm_open(SHM_NAME, O_CREAT | O_RDWR, 0666);
  if (shm_fd == -1) {
    perror("shm_open failed");
    exit(EXIT_FAILURE);
  }

  // Configure the size of the shared memory
  if (ftruncate(shm_fd, sizeof(SharedData)) == -1) {
    perror("ftruncate failed");
    exit(EXIT_FAILURE);
  }

  // Map the shared memory into the process's address space
  shared_data = (SharedData *)mmap(
      NULL, sizeof(SharedData), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
  if (shared_data == MAP_FAILED) {
    perror("mmap failed");
    exit(EXIT_FAILURE);
  }

  // Initialize the mutex attributes to work across multiple processes
  pthread_mutexattr_init(&mutex_attr);
  pthread_mutexattr_setpshared(&mutex_attr, PTHREAD_PROCESS_SHARED);

  // Initialize the mutex inside the shared memory
  if (pthread_mutex_init(&shared_data->mutex, &mutex_attr) != 0) {
    perror("pthread_mutex_init failed");
    exit(EXIT_FAILURE);
  }
  pthread_mutexattr_destroy(
      &mutex_attr); // Safe to destroy attributes after init

  // Initialize the shared counter
  shared_data->counter = 0;

  printf("Shared memory and mutex initialized successfully.\n");

  // ==========================================
  // 2. USAGE STAGE (Multi-process access)
  // ==========================================

  pid_t pid = fork();

  if (pid < 0) {
    perror("fork failed");
    exit(EXIT_FAILURE);
  } else if (pid == 0) {
    // --- CHILD PROCESS ---
    for (int i = 0; i < ITERATIONS; i++) {
      pthread_mutex_lock(&shared_data->mutex);
      shared_data->counter++;
      printf("Child  process incremented counter to: %d\n",
             shared_data->counter);
      pthread_mutex_unlock(&shared_data->mutex);
      sleep(1); // Simulate work
    }
    exit(EXIT_SUCCESS);
  } else {
    // --- PARENT PROCESS ---
    for (int i = 0; i < ITERATIONS; i++) {
      pthread_mutex_lock(&shared_data->mutex);
      shared_data->counter++;
      printf("Parent process incremented counter to: %d\n",
             shared_data->counter);
      pthread_mutex_unlock(&shared_data->mutex);
      sleep(1); // Simulate work
    }

    // Wait for child to finish before destroying
    wait(NULL);

    // ==========================================
    // 3. DESTRUCTION STAGE
    // ==========================================
    printf("\nFinal counter value is: %d (Expected: %d)\n",
           shared_data->counter, ITERATIONS * 2);

    // Destroy the mutex
    pthread_mutex_destroy(&shared_data->mutex);

    // Unmap the shared memory from the process
    if (munmap(shared_data, sizeof(SharedData)) == -1) {
      perror("munmap failed");
    }

    // Unlink (delete) the shared memory object from the system
    if (shm_unlink(SHM_NAME) == -1) {
      perror("shm_unlink failed");
    }

    printf("Shared memory and mutex destroyed successfully.\n");
  }

  return 0;
}
