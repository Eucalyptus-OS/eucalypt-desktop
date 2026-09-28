#pragma once

#include <stdint.h>

// One record as delivered by /dev/eventN; byte-compatible with Linux's
// userspace input_event ABI (24 bytes).
struct input_event {
    int64_t  tv_sec;
    int64_t  tv_usec;
    uint16_t type;
    uint16_t code;
    int32_t  value;
};

#define EV_SYN   0x00
#define EV_KEY   0x01
#define EV_REL   0x02
#define EV_ABS   0x03

#define REL_X 0
#define REL_Y 1

#define BTN_LEFT   272
#define BTN_RIGHT  273
#define BTN_MIDDLE 274

typedef struct {
    int x, y;            // current pointer position, in pixels
    int dx, dy;          // motion accumulated since the last input_poll()
    int buttons;         // bitmask of BTN_* currently held
    int wheel;           // wheel notches accumulated since the last input_poll()
} mouse_state_t;

typedef struct {
    uint16_t code;       // key code, EV_KEY
    int pressed;         // 1 press, 0 release, 2 auto-repeat
} key_event_t;

// Opens /dev/event0 (keyboard) and /dev/event1 (mouse) in non-blocking mode.
int input_init(void);
void input_release(void);

// Drains both devices without blocking. Returns the number of events stored.
int input_poll(void);

// Keyboard events from the last input_poll(), oldest first.
int input_keys(key_event_t *out, int max);

// Pointer state as of the last input_poll().
const mouse_state_t *input_mouse(void);
