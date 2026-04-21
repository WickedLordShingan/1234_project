#include "render.h"
#include <math.h>
#include <ncurses.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// digits for scrolling effect

static const char *digits[10][11] = {
    {"   #########   ", "  ###########  ", " ###       ### ", " ###       ### ",
     " ###       ### ", " ###       ### ", " ###       ### ", " ###       ### ",
     " ###       ### ", "  ###########  ", "   #########   "},
    {"       ###     ", "     #####     ", "   #######     ", " #########     ",
     "     #####     ", "     #####     ", "     #####     ", "     #####     ",
     "     #####     ", " ############# ", " ############# "},
    {"  ###########  ", " ############# ", "           ### ", "           ### ",
     "  ###########  ", " ###########   ", " ###           ", " ###           ",
     " ############# ", " ############# ", " ############# "},
    {"  ###########  ", " ############# ", "           ### ", "           ### ",
     "    ########## ", "    ########## ", "           ### ", "           ### ",
     " ############# ", " ############# ", "  ###########  "},
    {" ###       ### ", " ###       ### ", " ###       ### ", " ###       ### ",
     " ############# ", " ############# ", "           ### ", "           ### ",
     "           ### ", "           ### ", "           ### "},
    {" ############# ", " ############# ", " ###           ", " ###           ",
     " ###########   ", "  ###########  ", "           ### ", "           ### ",
     " ############# ", " ############# ", " ############# "},
    {"  ###########  ", " ############# ", " ###           ", " ###           ",
     " ###########   ", " ############# ", " ###       ### ", " ###       ### ",
     " ############# ", " ############# ", "  ###########  "},
    {" ############# ", " ############# ", "           ### ", "          ###  ",
     "        ###    ", "       ###     ", "     ###       ", "    ###        ",
     "  ###          ", " ###           ", " ###           "},
    {"  ###########  ", " ############# ", " ###       ### ", " ###       ### ",
     "  ###########  ", "  ###########  ", " ###       ### ", " ###       ### ",
     " ############# ", " ############# ", "  ###########  "},
    {"  ###########  ", " ############# ", " ###       ### ", " ###       ### ",
     " ############# ", " ############# ", "           ### ", "           ### ",
     " ############# ", " ############# ", "  ###########  "}};

// color pairs
#define CP_NORMAL 1
#define CP_PENDING 2
#define CP_CURSOR 3
#define CP_SATISFIED 4
#define CP_OWNFLOOR 5

#define BTN_W 6 //"[NNN] " per button since our max floor count is 1000 excluded
#define HEADER_ROWS 3
#define FOOTER_ROWS 2

// static becuase we dont want the user to interact with this
// 3 ncurses windows
static WINDOW *shaft_win = NULL;
static WINDOW *sep_win = NULL;
static WINDOW *panel_win = NULL;

static int shaft_w = 0; // it is 3/5ths of the total width
static int panel_x = 0; // start of the panel (shaft_w + 1)

// grid variables (floor selector)
static int g_total = 0;  // total_number_floors
static int g_cursor = 0; // floor hovered by the cursor
static int g_scroll = 0; // which row of buttons are at the top of the screen
                         // (when all of the buttons dont fir in the screen)
static int g_cols = 1;   // number of columns that can fit in a row

static volatile sig_atomic_t g_resized = 0;
static void on_sigwinch(int signal) {
  (void)signal;
  g_resized = 1;
}

/* window managemnet */

// handles resizes
static void rebuild(void) {

  // we clear the old windows from memory first
  if (shaft_win) {
    delwin(shaft_win);
    shaft_win = NULL;
  }
  if (sep_win) {
    delwin(sep_win);
    sep_win = NULL;
  }
  if (panel_win) {
    delwin(panel_win);
    panel_win = NULL;
  }

  endwin();
  refresh();
  clear();

  int h, w;
  getmaxyx(stdscr, h, w);
  // interseting fact ! getmaxyx is a macro so we dont need to pass the address
  //  #define getmaxyx(win,y,x)	(y = getmaxy(win), x = getmaxx(win))

  shaft_w = (w * 3) / 5;
  int panel_w = w - shaft_w - 1;

  g_cols = panel_w / BTN_W;
  if (g_cols < 1) {
    g_cols = 1;
  }

  panel_x = shaft_w + 1;

  shaft_win = newwin(
      h, shaft_w, 0,
      0); // height = h; width = shaft_w; top_left at the top_left of screen
  sep_win = newwin(h, 1, 0, shaft_w); // heigt = h; width = 1; shifted to the
                                      // right by the width of the shaft
  panel_win = newwin(h, panel_w, 0,
                     panel_x); // heigt = h; width = panel_w; shifted to the
                               // right by the shaft and the separator

  keypad(panel_win, TRUE);
  nodelay(panel_win, TRUE);

  g_resized = 0;
}

/* shaft */

static void draw_shaft(float pos, int myfloorid, int door_state,
                       int total_floors) {
  int h = getmaxy(shaft_win);
  int w = getmaxx(shaft_win);

  int sleft = w / 4;
  int sright = 3 * w / 4;

  // interior
  int ileft = sleft + 1;
  int iright = sright - 1;
  int iwidth = iright - ileft + 1;

  // door
  int dmid = ileft + iwidth / 2;

  // scrolling effect positions
  int ind_col = sright + 2;
  int ind_top = (h - 11) /
                2; // top of the scroll position (11 is the heigt of each glyph)
  // the below line is to check if we can afford the scroll animation with our
  // current space
  int show_art = (ind_col + 15 <= w) && (total_floors < 10);

  // handling edge cases
  if (total_floors < 1)
    total_floors = 1;
  if (!(pos >= 1.0f))
    pos = 1.0f;
  if (pos > (float)total_floors)
    pos = (float)total_floors;

  int floor_above = (int)pos;
  int floor_here = floor_above - 1;
  float frac = pos - (float)floor_above;
  // for the scroll effect we linearly interpolate the two floors' glyphs to get
  // a middleground sort of thing
  int blend = (int)roundf(frac * 11.0f);
  if (blend > 11)
    blend = 11;
  if (blend < 0)
    blend = 0;

  // setting the digits for the scroll effect
  int maxd = (total_floors < 10) ? total_floors : 9;
  int d_above = floor_above < 0 ? 0 : floor_above > maxd ? maxd : floor_above;
  int d_here = floor_here < 0 ? 0 : floor_here > maxd ? maxd : floor_here;

  int car_top = (int)((myfloorid + 1.0f - pos) * h);
  int car_bot = car_top + h;
  // TODO - check if h - 4 would display headers and footers

  werase(shaft_win);

  char line[4096];

  for (int row = 0; row < h; row++) {
    memset(line, ' ', (size_t)w);

    // ropes carrying the car by default
    line[sleft] = '|';
    line[sright] = '|';

    if (show_art) {
      int dr = row - ind_top;
      if (dr >= 0 && dr < 11) {
        const char *dl =
            (dr < blend) ? digits[d_above][dr] : digits[d_here][dr];
        memcpy(&line[ind_col], dl, 15);
      }
    } else if (total_floors >= 10 && row == h / 2) {
      char num[32];
      int n = snprintf(num, sizeof(num), "[ %d ]", floor_here);
      if (ind_col + n - 1 < w)
        memcpy(&line[ind_col], num, n);
    }

    if (row >= car_top && row < car_bot) {
      int lr = row - car_top;
      // this is the boundary of the car
      line[sleft] = '#';
      line[sright] = '#';

      if (lr == 0 || lr == h - 1) {
        // this is for header and footer I presume
        for (int c = ileft; c <= iright; c++)
          line[c] = '-';
      } else {
        if (door_state == 0) {
          for (int c = ileft; c < dmid; c++)
            line[c] = '<';
          for (int c = dmid; c <= iright; c++)
            line[c] = '>';
        } else {
          int p = (iwidth / 5 > 0) ? iwidth / 5 : 1;
          for (int c = ileft; c < ileft + p; c++)
            line[c] = '<';
          for (int c = iright - p + 1; c <= iright; c++)
            line[c] = '>';
        }
      }
    }

    else {
      if (row == h - 1)
        // footer ?
        for (int c = sleft + 1; c < sright; c++)
          line[c] = '=';
    }
    mvwaddnstr(shaft_win, row, 0, line, w);
  }
}

/* separator */

static void draw_sep(void) {
  // we erase whatever was here before so that we dont have previous frames data
  // showing itseld in the current frame
  werase(sep_win);
  int h = getmaxy(sep_win);
  for (int r = 0; r < h; r++)
    mvwaddch(sep_win, r, 0, ACS_VLINE);
}

/* panel */

static void draw_panel(int *states) {
  werase(panel_win);

  int ph = getmaxy(panel_win);
  int visible = ph - HEADER_ROWS - FOOTER_ROWS;
  if (visible < 1)
    visible = 1;

  wattron(panel_win, A_BOLD);
  mvwaddstr(panel_win, 0, 1, "FLOOR SELECT");
  wattroff(panel_win, A_BOLD);
  // arrow keys printing unicode
  mvwaddstr(panel_win, 1, 1, "\u2191\u2193\u2190\u2192  navigate");
  mvwaddstr(panel_win, 2, 1, "ENTER/SPC/click  request");

  // here g_cursor is the index of the button if the whole grid was a 1-d array
  //  adjust scroll so cursor stays visible
  int crow = g_cursor / g_cols;
  // if the position is above the screen then scroll up
  if (crow < g_scroll)
    g_scroll = crow;
  // if the position is below the screen then scroll down
  if (crow >= g_scroll + visible)
    g_scroll = crow - visible + 1;

  for (int vr = 0; vr < visible; vr++) {
    for (int col = 0; col < g_cols; col++) {
      // offset by the first row (achieves a scrolling effect if we cannot fit
      // everything in a single page)
      int idx = (g_scroll + vr) * g_cols + col;
      // we only need to render until the last floor
      if (idx >= g_total)
        break;

      // default color pair and style
      int cp = CP_NORMAL;
      int attr = A_DIM;

      // set the color and style accordingly
      if (states && states[idx] == 1) {
        cp = CP_PENDING;
        attr = A_BOLD;
      }
      // TODO - need only three states; pending, cursor and own floor
      if (states && states[idx] == 2) {
        cp = CP_SATISFIED;
        attr = A_NORMAL;
      }
      if (states && states[idx] == 3) {
        cp = CP_OWNFLOOR;
        attr = A_NORMAL;
      }
      if (idx == g_cursor) {
        cp = CP_CURSOR;
        attr = A_BOLD;
      }

      char btn[8];
      snprintf(btn, sizeof(btn), "[%3d]", idx);

      wattron(panel_win, COLOR_PAIR(cp) | attr);
      mvwaddstr(panel_win, HEADER_ROWS + vr, col * BTN_W, btn);
      wattroff(panel_win, COLOR_PAIR(cp) | attr);
    }
  }

  int last = (g_scroll + visible) * g_cols - 1;
  // last visible row
  if (last >= g_total)
    last = g_total - 1;
  // prints the range covered by the current screen along with the abosulte max
  // floor
  mvwprintw(panel_win, ph - 1, 1, "floors %d-%d / %d", g_scroll * g_cols, last,
            g_total - 1);
}

/* hit _checker */

static int panel_hit(int mx, int my) {
  int ph = getmaxy(panel_win);
  int visible = ph - HEADER_ROWS - FOOTER_ROWS;

  // convert to panel relative coordinates
  int px = mx - panel_x;
  int py = my;

  if (px < 0 || py < HEADER_ROWS || py >= HEADER_ROWS + visible)
    return -1;

  int vr = py - HEADER_ROWS;
  int col = px / BTN_W;
  if (col >= g_cols)
    return -1;

  int idx = (g_scroll + vr) * g_cols + col;
  if (idx < 0 || idx >= g_total)
    return -1;

  return idx;
}

/* PUBLIC API */

void render_cleanup(void) {
  if (shaft_win) {
    delwin(shaft_win);
    shaft_win = NULL;
  }
  if (sep_win) {
    delwin(sep_win);
    sep_win = NULL;
  }
  if (panel_win) {
    delwin(panel_win);
    panel_win = NULL;
  }
  endwin();
}

void render_init(int total_floors, int myfloorid) {
  g_total = total_floors;
  g_cursor = myfloorid;

  initscr();
  cbreak();
  // disables line break buffering and turns of interpreting control chars
  // probably
  noecho();
  curs_set(0);

  if (has_colors()) {
    start_color();
    use_default_colors();
    // default bg color is used
    init_pair(CP_NORMAL, COLOR_WHITE, -1);
    init_pair(CP_PENDING, COLOR_RED, -1);
    init_pair(CP_CURSOR, COLOR_BLACK, COLOR_WHITE);
    init_pair(CP_SATISFIED, COLOR_GREEN, -1);
    init_pair(CP_OWNFLOOR, COLOR_YELLOW, -1);
  }

  // dont ignore mouse input
  mousemask(BUTTON1_CLICKED | BUTTON1_PRESSED, NULL);

  signal(SIGWINCH, on_sigwinch);
  rebuild();
  atexit(render_cleanup);
  // at exit we need to give back all the memory back to the kernel
}

void render_draw(float pos, int myfloorid, int door_state, int total_floors,
                 int *states) {
  // handle resizes
  if (g_resized)
    rebuild();
  draw_shaft(pos, myfloorid, door_state, total_floors);
  draw_sep();
  draw_panel(states);
  // copies the changes to the virtual screen
  wnoutrefresh(shaft_win);
  wnoutrefresh(sep_win);
  wnoutrefresh(panel_win);
  // when doupdate is called it pushes the changes to the real screen
}

void render_refresh(void) { doupdate(); }

int render_poll_input(void) {
  if (g_resized)
    rebuild();

  int ph = getmaxy(panel_win);
  int visible = ph - HEADER_ROWS - FOOTER_ROWS;
  if (visible < 1)
    visible = 1;

  int ch = wgetch(panel_win);
  if (ch == ERR)
    return -1;

  switch (ch) {
  case KEY_UP:
    if (g_cursor - g_cols >= 0)
      g_cursor -= g_cols;
    break;
  case KEY_DOWN:
    if (g_cursor + g_cols < g_total)
      g_cursor += g_cols;
    break;
  case KEY_LEFT:
    if (g_cursor > 0)
      g_cursor--;
    break;
  case KEY_RIGHT:
    if (g_cursor < g_total - 1)
      g_cursor++;
    break;
  case '\n':
  case KEY_ENTER:
  case ' ':
    return g_cursor;
  case KEY_MOUSE: {
    MEVENT ev;
    if (getmouse(&ev) == OK)
      return panel_hit(ev.x, ev.y);
    break;
  }
  case KEY_RESIZE:
    rebuild();
    break;
  case 'q':
  case 'Q':
    render_cleanup();
    exit(0);
  }

  return -1;
}
