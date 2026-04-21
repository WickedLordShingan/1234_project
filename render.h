#pragma once

/*
 * states[i] — 0 = no request, 1 = pending (red), 2 = satisfied (green)
 * child.c fills this from SHM and passes it in every frame.
 */

void render_init(int total_floors, int myfloorid);
void render_cleanup(void);
void render_draw(float pos, int myfloorid, int door_state, int total_floors,
                 int *states);
void render_refresh(void);
int render_poll_input(
    void); /* floor index on keypress or click, -1 otherwise */
