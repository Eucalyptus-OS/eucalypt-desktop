// ex_shapes -- one of every drawing primitive gfx offers.
//
// Useful as a reference and as a way to confirm that clipping, alpha and the
// line/circle helpers behave as documented. The window is redrawn only when an
// event arrives or when a key toggles something, so it costs nothing while idle.

#include "gui_client.h"
#include "examples.h"

#include <stdio.h>

#define WIN_W 480
#define WIN_H 340

int main(void) {
    if (gui_connect() != 0) {
        printf("ex_shapes: no compositor\n");
        return 1;
    }

    gui_window_t *win = gui_window_create("shapes", 60, 50, WIN_W, WIN_H);
    if (!win) {
        printf("ex_shapes: could not create a window\n");
        return 1;
    }
    printf("ex_shapes: window %u is %dx%d\n", win->id, win->w, win->h);

    gfx_surface_t s;
    gfx_surface_init(&s, win->pixels, win->w, win->h, 0);

    int show_clip = 0;      // 'c' toggles a clip rect
    int dirty = 1;

    while (1) {
        if (dirty) {
            gfx_clear(&s, EX_BG);

            ex_draw_label(&s, "ex_shapes: gfx primitives", 10, 10, EX_FG);
            ex_draw_label(&s, "c: clip   space: blur   q: quit", 10, 24, EX_DIM);

            // Rectangles. gfx_fill_rect and friends all take x/y/w/h, and
            // negative extents are fine, which is handy for centring.
            gfx_fill_rect(&s, 20, 50, 90, 40, ex_rgb(0xC0, 0x40, 0x40));
            gfx_rect_outline(&s, 20, 50, 90, 40, ex_rgb(0xFF, 0xFF, 0xFF));
            gfx_fill_rect_alpha(&s, 26, 56, 90, 40, ex_rgb(0x40, 0xC0, 0x60), 140);

            // Lines, thin and thick. Thick lines get round-ish caps.
            gfx_line(&s, 20, 110, 110, 110, ex_rgb(0xFF, 0xD0, 0x40));
            gfx_line_thick(&s, 20, 125, 110, 145, ex_rgb(0x40, 0xC0, 0xC0), 5);

            // Circles, filled and outlined.
            gfx_circle(&s, 160, 75, 24, ex_rgb(0x80, 0x60, 0xD0));
            gfx_circle_outline(&s, 160, 75, 32, ex_rgb(0xFF, 0xFF, 0xFF));

            // An offscreen sprite, blitted twice. Blitting with alpha lets the
            // background show through.
            static uint32_t sprite_px[32 * 32];
            gfx_surface_t sprite;
            gfx_surface_init(&sprite, sprite_px, 32, 32, 0);
            gfx_clear(&sprite, ex_rgba(0xFF, 0xFF, 0xFF, 0));
            gfx_circle(&sprite, 16, 16, 12, ex_rgba(0xFF, 0xC0, 0x40, 200));
            gfx_fill_rect(&sprite, 12, 12, 8, 8, ex_rgba(0x20, 0x20, 0x30, 220));
            gfx_blit_alpha(&s, &sprite, 210, 50);
            gfx_blit(&s, &sprite, 250, 50);
            ex_draw_label(&s, "blit_alpha   blit", 200, 86, EX_DIM);

            // A row of pixels, the lowest level of drawing.
            for (int i = 0; i < 60; i++)
                gfx_pixel(&s, 20 + i, 160,
                          ex_rgb(255 - i * 4, 80 + i * 2, 120));

            // Blending by hand, for the case where you want one pixel of it.
            gfx_color_t under = gfx_get_pixel(&s, 20, 180);
            gfx_pixel(&s, 20, 180, gfx_blend(under, ex_rgba(0xFF, 0xFF, 0xFF, 128)));

            if (show_clip) {
                // Clip everything after this to one region. The outline shows
                // where the boundary is; shapes crossing it get cut off.
                gfx_rect_t region = { 300, 40, 150, 120 };
                gfx_rect_outline(&s, region.x, region.y, region.w, region.h,
                                 ex_rgb(0xFF, 0xFF, 0x00));
                gfx_set_clip(&s, &region);

                gfx_fill_rect(&s, 250, 20, 260, 160, ex_rgb(0x20, 0x60, 0xA0));
                gfx_circle(&s, 360, 100, 45, ex_rgb(0xA0, 0x40, 0xC0));
                gfx_line_thick(&s, 240, 170, 520, 30, ex_rgb(0xFF, 0xFF, 0xFF), 6);

                gfx_reset_clip(&s);
            } else {
                ex_draw_label(&s, "press c to clip", 300, 170, EX_DIM);
            }

            // Drawing outside the window is safe: every primitive is clipped.
            gfx_circle(&s, -20, WIN_H - 20, 40, ex_rgb(0x80, 0x80, 0x80));
            gfx_fill_rect(&s, WIN_W - 30, 10, 60, 60, ex_rgb(0x50, 0x50, 0x70));

            gui_window_damage(win, 0, 0, win->w, win->h);
            dirty = 0;
        }

        ex_event_t ev;
        if (!ex_wait(&ev, 1000))
            continue;

        if (ev.type == EX_KEY && ev.state) {
            int k = ex_key_char(ev.key);
            if (k == 'q' || k == 'Q' || ev.key == EX_KEY_ESC)
                break;
            if (k == 'c' || k == 'C') {
                show_clip = !show_clip;
                dirty = 1;
            } else if (k == ' ') {
                // A full-window translucent wash, demonstrating that alpha
                // composites against whatever is already there.
                gfx_fill_rect_alpha(&s, 0, 0, win->w, win->h,
                                    ex_rgb(0x20, 0x30, 0x80), 70);
                gui_window_damage(win, 0, 0, win->w, win->h);
            }
        } else if (ev.type == EX_MOTION) {
            // Motion marks the window for a repaint, so following the pointer
            // is just a matter of setting dirty again.
            dirty = 1;
        } else if (ev.type == EX_CLOSE) {
            break;
        }
    }

    gui_window_destroy(win);
    printf("ex_shapes: closed\n");
    return 0;
}
