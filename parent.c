#include "parent.h"
#include <fcntl.h>
#include <math.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>

#define FAILURE -1
#define SUCCESS 0
#define MAX_VELOCITY 2.0f
#define DECEL_DISTANCE 1.5f
#define DOOR_OPEN_TICKS 50

static int shm_fd = -1;
static LiftState *shared_data = NULL;
static pthread_mutexattr_t mutex_attr;
static volatile sig_atomic_t child_died = 0;

static void sigchld_handler(int sig) {
  (void)sig;
  child_died = 1;
}

static const char *terminals[][3] = {
    {"/usr/bin/xterm", "xterm", "-e"},
    {"/usr/bin/alacritty", "alacritty", "-e"},
    {"/usr/bin/kitty", "kitty", "--"},
    {"/usr/bin/gnome-terminal", "gnome-terminal", "--"},
    {"/usr/bin/xfce4-terminal", "xfce4-terminal", "-e"},
    {NULL, NULL, NULL}};

static void spawn_terminal(const char *floor_arg) {
  for (int i = 0; terminals[i][0] != NULL; i++) {
    if (access(terminals[i][0], X_OK) == 0)
      execl(terminals[i][0], terminals[i][1], terminals[i][2], "./child",
            floor_arg, NULL);
  }
  perror("no compatible terminal found");
  exit(EXIT_FAILURE);
}

int init_shm(int total_floors) {
  shm_unlink(SHM_NAME);

  shm_fd = shm_open(SHM_NAME, O_CREAT | O_RDWR, 0666);
  if (shm_fd == -1) {
    perror("shm_open");
    return FAILURE;
  }

  if (ftruncate(shm_fd, sizeof(LiftState)) == -1) {
    perror("ftruncate");
    close(shm_fd);
    shm_unlink(SHM_NAME);
    return FAILURE;
  }

  shared_data = (LiftState *)mmap(
      NULL, sizeof(LiftState), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
  if (shared_data == MAP_FAILED) {
    perror("mmap");
    close(shm_fd);
    shm_unlink(SHM_NAME);
    return FAILURE;
  }

  memset(shared_data, 0, sizeof(LiftState));
  shared_data->total_floors = total_floors;
  shared_data->position = 1.0f;
  shared_data->direction = DIR_IDLE;
  shared_data->door_state = DOOR_CLOSED;

  for (int i = 0; i < MAX_FLOORS; i++)
    shared_data->floor_data[i].pid = -1;

  pthread_mutexattr_init(&mutex_attr);
  pthread_mutexattr_setpshared(&mutex_attr, PTHREAD_PROCESS_SHARED);
  pthread_mutexattr_setrobust(&mutex_attr, PTHREAD_MUTEX_ROBUST);

  if (pthread_mutex_init(&shared_data->mutex, &mutex_attr) != 0) {
    perror("pthread_mutex_init");
    pthread_mutexattr_destroy(&mutex_attr);
    munmap(shared_data, sizeof(LiftState));
    close(shm_fd);
    shm_unlink(SHM_NAME);
    return FAILURE;
  }

  pthread_mutexattr_destroy(&mutex_attr);
  return SUCCESS;
}

void init_windows(int total_floors) {
  signal(SIGCHLD, sigchld_handler);

  for (int i = 0; i < total_floors; i++) {
    pid_t pid = fork();
    if (pid == -1) {
      perror("fork");
      continue;
    }
    if (pid == 0) {
      char floor_arg[16];
      snprintf(floor_arg, sizeof(floor_arg), "%d", i);
      spawn_terminal(floor_arg);
    }

    pthread_mutex_lock(&shared_data->mutex);
    shared_data->floor_data[i].pid = pid;
    shared_data->floor_data[i].floor = i;
    pthread_mutex_unlock(&shared_data->mutex);
  }
}

// to ignore the dead children
// void reap_dead_children(void) {
//   int status;
//   pid_t dead;
//
//   while ((dead = waitpid(-1, &status, WNOHANG)) > 0) {
//     pthread_mutex_lock(&shared_data->mutex);
//
//     for (int i = 0; i < shared_data->total_floors; i++) {
//       if (shared_data->floor_data[i].pid != dead)
//         continue;
//
//       shared_data->floor_data[i].pid = -1;
//       memset(shared_data->floor_data[i].requests, 0,
//              sizeof(shared_data->floor_data[i].requests));
//       shared_data->floor_data[i].up_pressed = 0;
//       shared_data->floor_data[i].down_pressed = 0;
//       shared_data->queue[i] = 0;
//       break;
//     }
//
//     pthread_mutex_unlock(&shared_data->mutex);
//   }
// }
//
static int has_requests(void) {
  int total = shared_data->total_floors;
  for (int i = 0; i < total; i++) {
    if (shared_data->queue[i])
      return 1;
    if (shared_data->floor_data[i].up_pressed)
      return 1;
    if (shared_data->floor_data[i].down_pressed)
      return 1;
  }
  return 0;
}

static int nearest_request_index(void) {
  float pos = shared_data->position;
  int total = shared_data->total_floors;
  int dir = shared_data->direction;
  int best_ahead = -1, best_behind = -1;
  float best_ahead_d = 1e9f, best_behind_d = 1e9f;

  for (int i = 0; i < total; i++) {
    int has_queue = shared_data->queue[i];
    int has_up = shared_data->floor_data[i].up_pressed;
    int has_down = shared_data->floor_data[i].down_pressed;
    if (!has_queue && !has_up && !has_down)
      continue;

    float floor_pos = (float)(i + 1);
    float d = fabsf(pos - floor_pos);

    int same_dir_call =
        (dir == DIR_UP && has_up) || (dir == DIR_DOWN && has_down);
    int geom_ahead = (dir == DIR_UP && floor_pos >= pos) ||
                     (dir == DIR_DOWN && floor_pos <= pos);
    int ahead =
        (dir == DIR_IDLE) || (geom_ahead && (has_queue || same_dir_call));

    if (ahead) {
      if (d < best_ahead_d) {
        best_ahead_d = d;
        best_ahead = i;
      }
    } else {
      if (d < best_behind_d) {
        best_behind_d = d;
        best_behind = i;
      }
    }
  }

  return (best_ahead != -1) ? best_ahead : best_behind;
}

static int should_accommodate(int floor_index) {
  if (shared_data->direction == DIR_UP &&
      shared_data->floor_data[floor_index].up_pressed)
    return 1;
  if (shared_data->direction == DIR_DOWN &&
      shared_data->floor_data[floor_index].down_pressed)
    return 1;
  return 0;
}

static void absorb_floor_requests(int floor_index) {
  int total = shared_data->total_floors;
  for (int i = 0; i < total; i++) {
    int req = shared_data->floor_data[floor_index].requests[i];
    if (shared_data->direction == DIR_UP && req == DIR_UP) {
      if (shared_data->floor_data[i].pid != -1)
        shared_data->queue[i] = 1;
      shared_data->floor_data[floor_index].requests[i] = 0;
    } else if (shared_data->direction == DIR_DOWN && req == DIR_DOWN) {
      if (shared_data->floor_data[i].pid != -1)
        shared_data->queue[i] = 1;
      shared_data->floor_data[floor_index].requests[i] = 0;
    }
  }
  if (shared_data->direction == DIR_UP)
    shared_data->floor_data[floor_index].up_pressed = 0;
  else if (shared_data->direction == DIR_DOWN)
    shared_data->floor_data[floor_index].down_pressed = 0;
}

void stopping_actions(void) {
  int cur_pos = (int)roundf(shared_data->position);
  int cur_data = cur_pos - 1;

  shared_data->velocity = 0.0f;
  shared_data->door_state = DOOR_OPEN;
  shared_data->queue[cur_data] = 0;

  if (should_accommodate(cur_data))
    absorb_floor_requests(cur_data);

  shared_data->floor_data[cur_data].up_pressed = 0;
  shared_data->floor_data[cur_data].down_pressed = 0;
}

void tele_update(float acceleration) {
  static int door_ticks = 0;

  // if (child_died) {
  //   child_died = 0;
  //   reap_dead_children();
  // }

  pthread_mutex_lock(&shared_data->mutex);

  if (shared_data->door_state == DOOR_OPEN) {
    if (++door_ticks >= DOOR_OPEN_TICKS) {
      shared_data->door_state = DOOR_CLOSED;
      door_ticks = 0;
    }
    pthread_mutex_unlock(&shared_data->mutex);
    return;
  }

  float pos = shared_data->position;
  int cur_pos = (int)roundf(pos);
  int cur_data = cur_pos - 1;
  int at_floor = fabsf(pos - (float)cur_pos) < 0.05f;

  // LIFT IS IDLE
  if (shared_data->direction == DIR_IDLE) {
    if (!has_requests()) {
      shared_data->velocity = 0.0f;
      pthread_mutex_unlock(&shared_data->mutex);
      return;
    }

    int target = nearest_request_index();
    if (target == -1) {
      pthread_mutex_unlock(&shared_data->mutex);
      return;
    }

    if (target == cur_data) {
      shared_data->direction =
          shared_data->floor_data[cur_data].up_pressed ? DIR_UP : DIR_DOWN;
      stopping_actions();
      shared_data->direction = DIR_IDLE;
    } else {
      shared_data->direction = (target > cur_data) ? DIR_UP : DIR_DOWN;
    }

    pthread_mutex_unlock(&shared_data->mutex);
    return;
  }

  // LIFT IS MOVING
  if (at_floor) {
    int stopped = 0;

    if (shared_data->queue[cur_data]) {
      stopping_actions();
      stopped = 1;
    } else if (should_accommodate(cur_data)) {
      stopping_actions();
      stopped = 1;
    } else {
      int nearest = nearest_request_index();
      if (nearest == cur_data) {
        if (shared_data->floor_data[cur_data].up_pressed)
          shared_data->direction = DIR_UP;
        else if (shared_data->floor_data[cur_data].down_pressed)
          shared_data->direction = DIR_DOWN;
        stopping_actions();
        stopped = 1;
      }
    }

    if (stopped) {
      if (has_requests()) {
        int next = nearest_request_index();
        if (next == -1 || next == cur_data)
          shared_data->direction = DIR_IDLE;
        else
          shared_data->direction = (next > cur_data) ? DIR_UP : DIR_DOWN;
      } else {
        shared_data->direction = DIR_IDLE;
      }
      pthread_mutex_unlock(&shared_data->mutex);
      return;
    }
  }

  int target = nearest_request_index();
  if (target == -1) {
    shared_data->direction = DIR_IDLE;
    shared_data->velocity = 0.0f;
    pthread_mutex_unlock(&shared_data->mutex);
    return;
  }

  float target_pos = (float)(target + 1);
  float dist = fabsf(pos - target_pos);

  if (dist < DECEL_DISTANCE)
    shared_data->velocity -= acceleration;
  else
    shared_data->velocity += acceleration;

  if (shared_data->velocity > MAX_VELOCITY)
    shared_data->velocity = MAX_VELOCITY;
  if (shared_data->velocity < 0.05f)
    shared_data->velocity = 0.05f;

  shared_data->position +=
      shared_data->velocity * (float)shared_data->direction;

  int total = shared_data->total_floors;
  if (shared_data->position < 1.0f)
    shared_data->position = 1.0f;
  if (shared_data->position > (float)total)
    shared_data->position = (float)total;

  pthread_mutex_unlock(&shared_data->mutex);
}

int child_health(void) {
  pthread_mutex_lock(&shared_data->mutex);
  int alive = 0;
  for (int i = 0; i < shared_data->total_floors; i++) {
    if (shared_data->floor_data[i].pid != -1)
      alive++;
  }
  pthread_mutex_unlock(&shared_data->mutex);
  return alive;
}

void mem_cleanup(void) {
  if (shared_data && shared_data != MAP_FAILED) {
    pthread_mutex_destroy(&shared_data->mutex);
    munmap(shared_data, sizeof(LiftState));
    shared_data = NULL;
  }
  if (shm_fd != -1) {
    close(shm_fd);
    shm_unlink(SHM_NAME);
    shm_fd = -1;
  }
}
