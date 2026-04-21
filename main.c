#include "parent.h"
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int init_shm(int total_floors);
void init_windows(int total_floors);
void tele_update(float acceleration);
int child_health(void);

volatile sig_atomic_t keep_running = 1;

static void on_signal(int sig) {
  (void)sig;
  keep_running = 0;
}

int main(int argc, char *argv[]) {
  if (argc != 2) {
    fprintf(stderr, "Usage: %s <number_of_floors>\n", argv[0]);
    return EXIT_FAILURE;
  }

  int floors = atoi(argv[1]);
  if (floors <= 0 || floors > 64) {
    fprintf(stderr, "Please enter a valid number of floors (1-64).\n");
    return EXIT_FAILURE;
  }

  atexit(mem_cleanup);

  struct sigaction sa;
  memset(&sa, 0, sizeof(sa));
  sa.sa_handler = on_signal;
  sigemptyset(&sa.sa_mask);
  sigaction(SIGINT, &sa, NULL);
  sigaction(SIGTERM, &sa, NULL);

  if (init_shm(floors) == -1) {
    return EXIT_FAILURE;
  }

  init_windows(floors);

  usleep(500000);

  while (keep_running && child_health() > 0) {
    tele_update(0.1f);
    usleep(50000);
  }

  printf("\nExiting. Memory cleanup triggered via atexit()...\n");
  return EXIT_SUCCESS;
}
