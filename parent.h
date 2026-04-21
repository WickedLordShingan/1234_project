#ifndef PARENT_H
#define PARENT_H

#include "common.h"
#include <pthread.h>

// init
//
// mem_cleanup
//
// child_cleanup
//
// tele_update
//
// stopping_actions (called when a request in the queue is the current floor)
//
// add_a_floor (if Im short of the 600 line limit)
//
// child_health check

int init_shm(int total_floors);
void init_windows(int total_floors);
void tele_update(float acceleration);
// void read_data(void);
//
void mem_cleanup(void);
void stopping_actions();
void add_floor(int total_floors, int addition_floors);
int child_health();

#endif
