// ex_text -- using a font with gfx.
//
// gfx ships no font data on purpose: a font is an application decision, and the
// library only needs to know how to turn a codepoint into a bitmap. That
// contract is a struct of callbacks, which this example fills in using the
// shared 5x7 font from examples.c.
//
// The other thing worth showing is the difference between the text APIs:
//   gfx_text         draw, and get the pen position back for chaining
//   gfx_text_width   how wide a string would be, for centring
//   gfx_line_height  baseline to baseline, for laying out paragraphs
//
// Nothing animates here, so the window is drawn once and then left alone until
// an event arrives.

#include "gui_client.h"
#include "examples.h"

#include <stdio.h>

#define WIN_W 440
#define WIN_H 300

static const char *const SAMPLE =
    "gfx_text draws with a baseline, not a top-left corner, so every y is\n"
    "measured up from the line the letters sit on. ex_draw_label() wraps\n"
    "that for you if you would rather think in top-left coordinates.";

int main(void) {
    if (gui_connect() != 0) {
        printf("ex_text: no compositor\n");
        return 1;
    }

    gui_window_t *win = gui_window_create("text", 200, 120, WIN_W, WIN_H);
    if (!win) {
        printf("ex_text: could not create a window\n");
        return 1;
    }
    printf("ex_text: window %u is %dx%d\n", win->id, win->w, win->h);

    gfx_surface_t s;
    gfx_surface_init(&s, win->pixels, win->w, win->h, 0);
    gfx_clear(&s, EX_BG);

    const gfx_font_t *font = ex_font();

    ex_draw_label(&s, "ex_text: fonts are pluggable", 12, 12, EX_FG);

    // A row showing every colour blend the same glyph can take, which is a
    // quick way to see that text honours the surface's clip and alpha.
    const gfx_color_t inks[] = {
        ex_rgb(0xFF, 0x60, 0x60), ex_rgb(0x60, 0xFF, 0x60),
        ex_rgb(0x60, 0xA0, 0xFF), ex_rgb(0xFF, 0xD0, 0x40),
    };
    for (int i = 0; i < 4; i++) {
        gfx_fill_rect(&s, 14 + i * 104, 34, 96, 22, ex_rgb(0x24, 0x26, 0x30));
        ex_draw_label(&s, "Ag", 20 + i * 104, 38, inks[i]);
    }

    // Centring, using gfx_text_width to find out how much room to leave.
    ex_draw_label_centered(&s, "centred with gfx_text_width", 0, win->w, 66, EX_FG);

    // Multi-line layout: walk the string and use gfx_line_height() for the
    // leading. gfx_text returns the pen x, so the next call can continue it.
    int x = 14, y = 92;
    const gfx_font_t *f = font;
    int lh = gfx_line_height(f);
    for (const char *p = SAMPLE; *p; ) {
        if (*p == '\n') {
            x = 14;
            y += lh;
            p++;
            continue;
        }
        x = gfx_text(&s, f, x, y, p, EX_DIM);
        // Advance our position past this one codepoint. gfx_utf8_len returns
        // the byte length of the sequence, so this stays correct for anything
        // above ASCII.
        p += gfx_utf8_len(p);
    }

    // Measuring, so the numbers are visible rather than merely available.
    ex_draw_label(&s, "measurements", 14, 210, EX_FG);
    int tw = gfx_text_width(f, "Wiiiiiiiiii");
    char buf[96];
    snprintf(buf, sizeof buf, "gfx_text_width(\"Wiiiiiiiiii\") = %d", tw);
    ex_draw_label(&s, buf, 14, 226, EX_DIM);
    snprintf(buf, sizeof buf, "gfx_line_height() = %d", lh);
    ex_draw_label(&s, buf, 14, 242, EX_DIM);
    snprintf(buf, sizeof buf, "gfx_utf8_len(\"e\") = %d", gfx_utf8_len("e"));
    ex_draw_label(&s, buf, 14, 258, EX_DIM);

    // Alpha text, to show that glyph coverage blends rather than replaces.
    ex_draw_label(&s, "half-transparent", 250, 210, ex_rgba(0xFF, 0xFF, 0xFF, 110));

    gui_window_damage(win, 0, 0, win->w, win->h);
    printf("ex_text: drew, now waiting for input\n");

    while (1) {
        ex_event_t ev;
        if (!ex_wait(&ev, 1000))
            continue;
        if (ev.type == EX_CLOSE)
            break;
        if (ev.type == EX_KEY && ev.state) {
            int k = ex_key_char(ev.key);
            if (k == 'q' || k == 'Q' || ev.key == EX_KEY_ESC)
                break;
        }
    }

    gui_window_destroy(win);
    printf("ex_text: closed\n");
    return 0;
}
