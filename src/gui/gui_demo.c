// A standalone client: it never opens /dev/fb0. It asks the compositor for a
// window, draws into the shared buffer it is handed, and reports damage.
//
// This is the shape every future app follows:
//   connect -> create window -> write pixels -> damage -> pump events

#include "gui_client.h"

#include <stdio.h>
#include <unistd.h>

#define WIN_W 420
#define WIN_H 260

// A simple diagonal gradient plus a solid band, so it is obvious on screen that
// the pixels really did travel from this process into the compositor's frame.
static void draw(uint32_t *px, int w, int h) {
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            uint32_t r = (uint32_t)(x * 255 / (w > 1 ? w - 1 : 1));
            uint32_t g = (uint32_t)(y * 255 / (h > 1 ? h - 1 : 1));
            px[y * w + x] = (0xFFu << 24) | (r << 16) | (g << 8) | 0x40u;
        }
    }
    for (int y = h / 3; y < h / 3 + 24 && y < h; y++)
        for (int x = 0; x < w; x++)
            px[y * w + x] = 0xFF3030FFu;
}

int main(void) {
    if (gui_connect() != 0) {
        printf("guitest: no compositor\n");
        return 1;
    }
    printf("guitest: connected to compositor\n");

    gui_window_t *win = gui_window_create("guitest", 220, 180, WIN_W, WIN_H);
    if (!win) {
        printf("guitest: window creation failed\n");
        return 1;
    }
    printf("guitest: got window id=%u at %dx%d pixels=%p\n",
           win->id, win->w, win->h, (void *)win->pixels);

    draw(win->pixels, win->w, win->h);
    gui_window_damage(win, 0, 0, win->w, win->h);
    printf("guitest: drew %dx%d and reported damage, windows=%d\n",
           win->w, win->h, gui_window_count());

    // Pump input so the demo exercises the routing path too.
    for (int i = 0; i < 200; i++) {
        gui_event_t ev;
        while (gui_poll_event(&ev)) {
            if (ev.type == GUI_EVT_BUTTON)
                printf("guitest: button %d %s at %d,%d\n", ev.button,
                       ev.state ? "down" : "up", ev.x, ev.y);
            else if (ev.type == GUI_EVT_KEY)
                printf("guitest: key %d %s\n", ev.button, ev.state ? "down" : "up");
            else if (ev.type == GUI_EVT_FOCUS)
                printf("guitest: focused=%d\n", ev.state);
        }
        usleep(20000);
    }

    printf("guitest: done\n");
    return 0;
}
