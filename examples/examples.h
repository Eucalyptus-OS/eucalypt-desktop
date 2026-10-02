#pragma once

// Helpers shared by the examples. These exist so each example file can stay
// short enough to read in one go and focus on what it is demonstrating.
//
// Nothing here is required to use gfx; it is all convenience.

#include "gui_client.h"     // src/gui/gui_client.h
#include <eucalypt/gfx.h>

// The gfx types, under short names. The examples read better with these, and
// it stays obvious which names belong to gfx itself and which to this header.
typedef gfx_color gfx_color_t;   // 0xAARRGGBB
typedef gfx_rect  gfx_rect_t;    // x, y, w, h
typedef gfx_font  gfx_font_t;    // pluggable font callbacks
typedef gfx_surface gfx_surface_t;

// --- colours -----------------------------------------------------------------

static inline gfx_color_t ex_rgb(uint8_t r, uint8_t g, uint8_t b) {
    return gfx_rgb(r, g, b);
}

static inline gfx_color_t ex_rgba(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    return gfx_rgba(r, g, b, a);
}

// A mid-grey used as a "nothing here" background.
#define EX_BG      ex_rgb(0x18, 0x1A, 0x22)
#define EX_FG      ex_rgb(0xE8, 0xEA, 0xF0)
#define EX_DIM     ex_rgb(0x88, 0x90, 0xA0)

// --- geometry ----------------------------------------------------------------

static inline int ex_point_in(int px, int py, gfx_rect_t r) {
    return px >= r.x && px < r.x + r.w && py >= r.y && py < r.y + r.h;
}

// --- input -------------------------------------------------------------------
//
// The examples do not care that events are named GUI_EVT_* or that keys arrive
// as keycodes. They only need a press/release, a coordinate, and a quit, so
// the event type is flattened here once instead of in every example.

typedef enum {
    EX_NONE,
    EX_KEY,       // a key went down or up
    EX_BUTTON,    // mouse button 0 went down or up
    EX_MOTION,    // the mouse moved
    EX_CLOSE      // the compositor asked this window to close
} ex_event_type_t;

typedef struct {
    ex_event_type_t type;
    int x, y;      // pointer position, relative to the window
    int state;     // 1 = pressed / going down, 0 = released
    int key;       // EX_KEY: the keycode
    int button;    // EX_BUTTON: which button
} ex_event_t;

// Translate a key event into a character, so examples can test for 'q' or '1'
// instead of raw evdev codes. Returns 0 when the key has no character meaning
// (arrows, function keys, and so on).
//
// The GUI protocol deliberately passes through the kernel's input codes rather
// than inventing an ASCII layer, so a real app that cares about more than
// letters wants these codes directly.
int ex_key_char(int keycode);

// The codes the examples treat specially.
#define EX_KEY_ESC   1
#define EX_KEY_ENTER 28
#define EX_KEY_TAB   15
#define EX_KEY_SPACE 57

// Find the compositor, waiting for it if it is not up yet.
//
// A client started at boot races the desktop: the compositor may not have
// created its control region yet, and a plain gui_connect() would fail and the
// client would exit. This retries for up to `timeout_ms`, so callers can just
// connect and not care about start order. Returns 0, or negative errno.
int ex_connect(int timeout_ms);

// Block until an event arrives, or until `idle_ms` has passed with nothing
// happening. Returns 1 and fills *out if an event was received, 0 on timeout.
//
// This is what keeps an idle example cheap: the main loop waits here instead of
// redrawing on a timer, so an untouched window costs nothing. A static example
// can pass a large value and will then wake only on input.
int ex_wait(ex_event_t *out, int idle_ms);

// Milliseconds since some arbitrary fixed point, for the rare example that
// needs to know how much time passed. Monotonic, so it is unaffected by
// wall-clock changes.
long ex_now_ms(void);

// --- text --------------------------------------------------------------------
//
// gfx deliberately ships no font data, because font choice is an application
// decision. For the examples we use the tiny 5x7 bitmap below, which is enough
// for short labels and is the same shape a real app would plug in.

extern const gfx_font_t *ex_font(void);

// Draw `text` with its top-left at (x, y). gfx_text positions by baseline, so
// this wrapper adds the ascent to make (x, y) mean what it looks like it means.
void ex_draw_label(gfx_surface *s, const char *text, int x, int y, gfx_color_t c);

// Draw `text` centred horizontally in [x, x + w).
void ex_draw_label_centered(gfx_surface *s, const char *text, int x, int w, int y,
                            gfx_color_t c);
