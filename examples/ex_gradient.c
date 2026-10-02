// ex_gradient -- the smallest useful gfx client.
//
// A window, a background gradient, and one rectangle that changes colour when
// you click it. Everything here follows the same four steps every gfx client
// uses:
//
//   1. connect to the compositor
//   2. ask for a window
//   3. write pixels into the buffer the compositor gave us
//   4. tell the compositor which part changed
//
// The pixel buffer is shared memory, so step 3 is a plain array write. There is
// no per-drawing-call IPC; the compositor picks the pixels up on its next frame.

#include "gui_client.h"
#include "examples.h"

#include <stdio.h>
#include <unistd.h>

#define WIN_W 360
#define WIN_H 240

int main(void) {
    if (gui_connect() != 0) {
        printf("ex_gradient: no compositor\n");
        return 1;
    }

    gui_window_t *win = gui_window_create("gradient", 120, 90, WIN_W, WIN_H);
    if (!win) {
        printf("ex_gradient: could not create a window\n");
        return 1;
    }
    printf("ex_gradient: window %u is %dx%d at %d,%d, pixels=%p\n",
           win->id, win->w, win->h, win->x, win->y, (void *)win->pixels);

    gfx_surface s;
    gfx_surface_init(&s, win->pixels, win->w, win->h, 0);

    gfx_rect_t swatch = { 120, 80, 120, 80 };
    gfx_color_t swatch_color = ex_rgb(0x3D, 0x7B, 0xF0);

    // Draw once up front, then only redraw when the state actually changes.
    // Nothing here animates on a timer, so an idle window does no work at all.
    int dirty = 1;
    while (1) {
        if (dirty) {
            for (int y = 0; y < win->h; y++) {
                int t = y * 255 / (win->h > 1 ? win->h - 1 : 1);
                gfx_hline(&s, 0, y, win->w, ex_rgb(t / 3, t / 2, 255 - t));
            }

            // A bordered box with its fill blended in, to show off alpha.
            gfx_fill_rect_alpha(&s, swatch.x, swatch.y, swatch.w, swatch.h,
                                swatch_color, 180);
            gfx_rect_outline(&s, swatch.x, swatch.y, swatch.w, swatch.h,
                             ex_rgb(0xFF, 0xFF, 0xFF));

            ex_draw_label(&s, "ex_gradient", 8, 8, ex_rgb(0xFF, 0xFF, 0xFF));
            ex_draw_label(&s, "click the box", 8, 22, ex_rgb(0xD0, 0xD8, 0xFF));

            // The whole window changed, so damage all of it. Damage is a
            // rectangle; if only part had changed you would pass just that part.
            gui_window_damage(win, 0, 0, win->w, win->h);
            dirty = 0;
        }

        // Wait for input. The timeout is only a safety net; a static example
        // does not need to wake up on its own.
        ex_event_t ev;
        if (!ex_wait(&ev, 1000))
            continue;

        if (ev.type == EX_BUTTON && ev.state && ex_point_in(ev.x, ev.y, swatch)) {
            // Clicking inside the swatch cycles it through a few colours.
            static const gfx_color_t palette[] = {
                0x3D7BF0, 0xF03D7B, 0x7BF03D, 0xF0C33D, 0xA83DF0
            };
            // Starts at 1 so the first click already changes something, rather
            // than re-selecting the colour the box already has.
            static int next = 1;
            swatch_color = ex_rgb((palette[next] >> 16) & 0xFF,
                                  (palette[next] >> 8) & 0xFF,
                                  palette[next] & 0xFF);
            next = (next + 1) % (int)(sizeof palette / sizeof palette[0]);
            dirty = 1;
        } else if (ev.type == EX_KEY && ev.state) {
            int k = ex_key_char(ev.key);
            if (k == 'q' || k == 'Q' || ev.key == EX_KEY_ESC)
                break;
        } else if (ev.type == EX_CLOSE) {
            break;
        }
    }

    gui_window_destroy(win);
    printf("ex_gradient: closed\n");
    return 0;
}
