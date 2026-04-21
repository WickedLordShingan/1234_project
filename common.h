#ifndef COMMON_H
#define COMMON_H

#include <pthread.h>
#define SHM_NAME "/lift_and_floor_data"
#define MAX_FLOORS 64

typedef enum { DOOR_CLOSED = 0, DOOR_MOVING = 1, DOOR_OPEN = 2 } DoorState;

typedef enum { DIR_DOWN = -1, DIR_IDLE = 0, DIR_UP = 1 } Direction;

typedef struct {
  pid_t pid;
  int floor;
  int requests[MAX_FLOORS];
  int up_pressed;
  int down_pressed;
} FloorSlot;

typedef struct {
  float position;
  float velocity;
  int door_state;
  int direction;
  int total_floors;
  int queue[MAX_FLOORS];
  FloorSlot floor_data[MAX_FLOORS];
  pthread_mutex_t mutex;
} LiftState;

#endif
