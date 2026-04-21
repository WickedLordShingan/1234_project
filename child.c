#include "common.h"
#include "render.h"
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

static LiftState *shm = NULL;
static int shm_fd = -1;
static int my_floor = 0;

static void cleanup(void) {
  render_cleanup();
  if (shm && shm != MAP_FAILED) {
    munmap(shm, sizeof(LiftState));
    shm = NULL;
  }
  if (shm_fd != -1) {
    close(shm_fd);
    shm_fd = -1;
  }
}

static void on_signal(int sig) {
  (void)sig;
  cleanup();
  _exit(0);
}

static void lock(void) {
  int rc = pthread_mutex_lock(&shm->mutex);
  if (rc == EOWNERDEAD)
    pthread_mutex_consistent(&shm->mutex);
}

static void unlock(void) { pthread_mutex_unlock(&shm->mutex); }

static void register_floor(void) {
  lock();
  shm->floor_data[my_floor].pid = getpid();
  shm->floor_data[my_floor].floor = my_floor;
  unlock();
}

static void handle_keypress(void) {
  int dest = render_poll_input();
  if (dest < 0 || dest == my_floor)
    return;

  int dir = (dest > my_floor) ? DIR_UP : DIR_DOWN;

  lock();
  shm->floor_data[my_floor].requests[dest] = dir;
  if (dir == DIR_UP)
    shm->floor_data[my_floor].up_pressed = 1;
  else
    shm->floor_data[my_floor].down_pressed = 1;
  unlock();
}

static void read_data(float *pos, int *door_state, int *total_floors,
                      int *states) {
  lock();

  *pos = shm->position;
  *door_state = shm->door_state;
  *total_floors = shm->total_floors;

  for (int i = 0; i < shm->total_floors; i++) {
    if (i == my_floor)
      states[i] = 3;
    else if (shm->floor_data[my_floor].requests[i] != 0 || shm->queue[i])
      states[i] = 1;
    else
      states[i] = 0;
  }

  unlock();
}

int main(int argc, char *argv[]) {
  if (argc < 2) {
    fprintf(stderr, "usage: child <floor_id>\n");
    return 1;
  }

  my_floor = atoi(argv[1]);
  if (my_floor < 0 || my_floor >= MAX_FLOORS) {
    fprintf(stderr, "child: floor_id %d out of range\n", my_floor);
    return 1;
  }

  shm_fd = shm_open(SHM_NAME, O_RDWR, 0666);
  if (shm_fd == -1) {
    perror("shm_open");
    return 1;
  }

  shm = (LiftState *)mmap(NULL, sizeof(LiftState), PROT_READ | PROT_WRITE,
                          MAP_SHARED, shm_fd, 0);
  if (shm == MAP_FAILED) {
    perror("mmap");
    close(shm_fd);
    return 1;
  }

  // handle interrupt signals by cleaning up before exiting
  struct sigaction sa;
  memset(&sa, 0, sizeof(sa));
  sa.sa_handler = on_signal;
  sigemptyset(&sa.sa_mask);
  sigaction(SIGTERM, &sa, NULL);
  sigaction(SIGINT, &sa, NULL);

  // even if everything goes with interruption then we need to cleanup at the
  // end
  atexit(cleanup);

  register_floor();

  render_init(shm->total_floors, my_floor);

  int states[MAX_FLOORS];
  float pos;
  int door_state;
  int total_floors;

  while (1) {
    read_data(&pos, &door_state, &total_floors, states);
    render_draw(pos, my_floor, door_state, total_floors, states);
    render_refresh();
    handle_keypress();
  }

  return 0;
}
