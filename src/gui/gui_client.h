#pragma once

#include <stdint.h>
#include "gui_protocol.h"

// Client side of the windowing protocol. A client never opens /dev/fb0.
//
//   gui_connect()                     find the compositor and claim a slot
//   gui_window_create(title,x,y,w,h)  get a window and a pixel buffer to draw in
//   gui_window_damage()               tell the compositor the window changed
//   gui_window_move()                 reposition the window
//   gui_poll_event()                  receive key/button/motion for your windows
//
// The buffer returned by gui_window_create() is shared memory: write pixels
// straight into it and the compositor picks them up on its next frame. There is
// no per-drawing-call IPC.

typedef struct {
    uint32_t  id;
    uint32_t *pixels;     // ARGB, one uint32 per pixel, row-major, no padding
    int       w, h;
    int       x, y;
    char      title[GUI_TITLE_LEN];
} gui_window_t;

// Connect to the running compositor. Returns 0, or negative errno.
int gui_connect(void);

// Create a window and map its backing store. Returns NULL on failure.
gui_window_t *gui_window_create(const char *title, int x, int y, int w, int h);

// Report that a rectangle of a window changed.
int gui_window_damage(gui_window_t *win, int x, int y, int w, int h);

// Move a window to an absolute screen position.
int gui_window_move(gui_window_t *win, int x, int y);

// Change a window's title.
int gui_window_set_title(gui_window_t *win, const char *title);

// Destroy a window.
int gui_window_destroy(gui_window_t *win);

// Fetch the next pending event for any of this client's windows.
// Returns 1 when *out was filled, 0 when there is nothing queued.
int gui_poll_event(gui_event_t *out);

// How many windows this client currently owns.
int gui_window_count(void);
