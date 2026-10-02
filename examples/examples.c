// Shared helpers for the examples. See examples.h for what these are for.
//
// The only real dependency here is the GUI client, and even that is avoidable:
// this file is the one place that knows the GUI's event names, so the examples
// themselves stay about drawing.

#include "examples.h"

#include <stdio.h>

// --- input -------------------------------------------------------------------

int ex_poll(ex_event_t *out) {
    gui_event_t ev;
    if (!gui_poll_event(&ev))
        return 0;

    switch (ev.type) {
    case GUI_EVT_KEY:
        out->type = EX_KEY;
        out->key  = ev.button;
        break;
    case GUI_EVT_BUTTON:
        out->type   = EX_BUTTON;
        out->button = ev.button;
        break;
    case GUI_EVT_MOTION:
        out->type = EX_MOTION;
        break;
    case GUI_EVT_CLOSE:
        out->type = EX_CLOSE;
        break;
    default:
        return 0;
    }

    out->x     = ev.x;
    out->y     = ev.y;
    out->state = ev.state;
    return 1;
}

// --- keys -------------------------------------------------------------------
//
// The kernel publishes PS/2 set-1 scancodes rather than ASCII, so pressing 'c'
// arrives as 0x2E (46), not as 'c' (99). Mapping them here means the examples
// can be read as "when the user presses c" instead of "when the user presses
// 46".
//
// The letters are deliberately a table and not a range: set-1 lays them out as
// Q..P (16..25), A..L (30..38), then Z..M (44..50), with punctuation wedged in
// between, so a contiguous range would map 'c' to 'q'.

int ex_key_char(int keycode) {
    switch (keycode) {
    // Letters, in the order the physical keys sit on the keyboard.
    case 16: return 'q'; case 17: return 'w'; case 18: return 'e';
    case 19: return 'r'; case 20: return 't'; case 21: return 'y';
    case 22: return 'u'; case 23: return 'i'; case 24: return 'o';
    case 25: return 'p';
    case 30: return 'a'; case 31: return 's'; case 32: return 'd';
    case 33: return 'f'; case 34: return 'g'; case 35: return 'h';
    case 36: return 'j'; case 37: return 'k'; case 38: return 'l';
    case 44: return 'z'; case 45: return 'x'; case 46: return 'c';
    case 47: return 'v'; case 48: return 'b'; case 49: return 'n';
    case 50: return 'm';
    // Number row.
    case  2: return '1'; case  3: return '2'; case  4: return '3';
    case  5: return '4'; case  6: return '5'; case  7: return '6';
    case  8: return '7'; case  9: return '8'; case 10: return '9';
    case 11: return '0';
    // Punctuation on the number row.
    case 12: return '-'; case 13: return '='; case 26: return '[';
    case 27: return ']'; case 43: return '\\';
    // Punctuation on the letter block.
    case 39: return ';'; case 40: return '\''; case 41: return '`';
    case 51: return ','; case 52: return '.'; case 53: return '/';
    // Whitespace and editing.
    case EX_KEY_SPACE:  return ' ';
    case EX_KEY_ENTER:  return '\n';
    case 14:            return '\b';   // backspace
    case EX_KEY_TAB:    return '\t';
    default:            return 0;
    }
}

// --- timing ------------------------------------------------------------------
//
// A desktop has no frame rate to hit. The compositor presents whatever is in
// shared memory when it gets around to it, and an application redraws when its
// own state changes, not on a schedule. So there is deliberately no frame
// counter and no "target 60fps" anywhere in the examples: one that is not
// animating sits in ex_wait() and draws nothing.

#include <time.h>
#include <unistd.h>

long ex_now_ms(void) {
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) == 0)
        return (long)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
    return 0;
}

int ex_connect(int timeout_ms) {
    int waited = 0;
    for (;;) {
        int rc = gui_connect();
        if (rc == 0) return 0;

        // Stop well short of the timeout, so a compositor that never appears
        // still produces an error rather than an apparently hung client.
        if (waited >= timeout_ms) return rc;
        usleep(20000);
        waited += 20;
    }
}

int ex_wait(ex_event_t *out, int idle_ms) {
    // Events arrive through shared memory, so there is nothing to block on
    // directly. Sleep in short slices and poll: that keeps input latency low
    // without needing a wakeup mechanism the protocol does not have.
    int slept = 0;
    for (;;) {
        if (ex_poll(out))
            return 1;
        if (slept >= idle_ms)
            return 0;
        int slice = idle_ms - slept > 4 ? 4 : idle_ms - slept;
        usleep((useconds_t)slice * 1000);
        slept += slice;
    }
}

// --- text --------------------------------------------------------------------

// A 5x7 bitmap font, one byte per column, bit 0 = top row. That is the classic
// "font5x7" layout. gfx wants row-major data, so glyph() transposes on the way
// out into a static buffer that stays valid until the next call, which is what
// the gfx_font contract asks for.
static const uint8_t font5x7[95][5] = {
    {0x00,0x00,0x00,0x00,0x00}, /* ' ' */
    {0x00,0x00,0x5F,0x00,0x00}, /* ! */
    {0x00,0x07,0x00,0x07,0x00}, /* " */
    {0x14,0x7F,0x14,0x7F,0x14}, /* # */
    {0x24,0x2A,0x7F,0x2A,0x12}, /* $ */
    {0x23,0x13,0x08,0x64,0x62}, /* % */
    {0x36,0x49,0x55,0x22,0x50}, /* & */
    {0x00,0x05,0x03,0x00,0x00}, /* ' */
    {0x00,0x1C,0x22,0x41,0x00}, /* ( */
    {0x00,0x41,0x22,0x1C,0x00}, /* ) */
    {0x14,0x08,0x3E,0x08,0x14}, /* * */
    {0x08,0x08,0x3E,0x08,0x08}, /* + */
    {0x00,0x50,0x30,0x00,0x00}, /* , */
    {0x08,0x08,0x08,0x08,0x08}, /* - */
    {0x00,0x60,0x60,0x00,0x00}, /* . */
    {0x20,0x10,0x08,0x04,0x02}, /* / */
    {0x3E,0x51,0x49,0x45,0x3E}, /* 0 */
    {0x00,0x42,0x7F,0x40,0x00}, /* 1 */
    {0x42,0x61,0x51,0x49,0x46}, /* 2 */
    {0x21,0x41,0x45,0x4B,0x31}, /* 3 */
    {0x18,0x14,0x12,0x7F,0x10}, /* 4 */
    {0x27,0x45,0x45,0x45,0x39}, /* 5 */
    {0x3C,0x4A,0x49,0x49,0x30}, /* 6 */
    {0x01,0x71,0x09,0x05,0x03}, /* 7 */
    {0x36,0x49,0x49,0x49,0x36}, /* 8 */
    {0x06,0x49,0x49,0x29,0x1E}, /* 9 */
    {0x00,0x36,0x36,0x00,0x00}, /* : */
    {0x00,0x56,0x36,0x00,0x00}, /* ; */
    {0x08,0x14,0x22,0x41,0x00}, /* < */
    {0x14,0x14,0x14,0x14,0x14}, /* = */
    {0x00,0x41,0x22,0x14,0x08}, /* > */
    {0x02,0x01,0x51,0x09,0x06}, /* ? */
    {0x32,0x49,0x79,0x41,0x3E}, /* @ */
    {0x7E,0x11,0x11,0x11,0x7E}, /* A */
    {0x7F,0x49,0x49,0x49,0x36}, /* B */
    {0x3E,0x41,0x41,0x41,0x22}, /* C */
    {0x7F,0x41,0x41,0x22,0x1C}, /* D */
    {0x7F,0x49,0x49,0x49,0x41}, /* E */
    {0x7F,0x09,0x09,0x09,0x01}, /* F */
    {0x3E,0x41,0x49,0x49,0x7A}, /* G */
    {0x7F,0x08,0x08,0x08,0x7F}, /* H */
    {0x00,0x41,0x7F,0x41,0x00}, /* I */
    {0x20,0x40,0x41,0x3F,0x01}, /* J */
    {0x7F,0x08,0x14,0x22,0x41}, /* K */
    {0x7F,0x40,0x40,0x40,0x40}, /* L */
    {0x7F,0x02,0x0C,0x02,0x7F}, /* M */
    {0x7F,0x04,0x08,0x10,0x7F}, /* N */
    {0x3E,0x41,0x41,0x41,0x3E}, /* O */
    {0x7F,0x09,0x09,0x09,0x06}, /* P */
    {0x3E,0x41,0x51,0x21,0x5E}, /* Q */
    {0x7F,0x09,0x19,0x29,0x46}, /* R */
    {0x46,0x49,0x49,0x49,0x31}, /* S */
    {0x01,0x01,0x7F,0x01,0x01}, /* T */
    {0x3F,0x40,0x40,0x40,0x3F}, /* U */
    {0x1F,0x20,0x40,0x20,0x1F}, /* V */
    {0x3F,0x40,0x38,0x40,0x3F}, /* W */
    {0x63,0x14,0x08,0x14,0x63}, /* X */
    {0x07,0x08,0x70,0x08,0x07}, /* Y */
    {0x61,0x51,0x49,0x45,0x43}, /* Z */
    {0x00,0x7F,0x41,0x41,0x00}, /* [ */
    {0x02,0x04,0x08,0x10,0x20}, /* backslash */
    {0x00,0x41,0x41,0x7F,0x00}, /* ] */
    {0x04,0x02,0x01,0x02,0x04}, /* ^ */
    {0x40,0x40,0x40,0x40,0x40}, /* _ */
    {0x00,0x01,0x02,0x04,0x00}, /* ` */
    {0x20,0x54,0x54,0x54,0x78}, /* a */
    {0x7F,0x48,0x44,0x44,0x38}, /* b */
    {0x38,0x44,0x44,0x44,0x20}, /* c */
    {0x38,0x44,0x44,0x48,0x7F}, /* d */
    {0x38,0x54,0x54,0x54,0x18}, /* e */
    {0x08,0x7E,0x09,0x01,0x02}, /* f */
    {0x0C,0x52,0x52,0x52,0x3E}, /* g */
    {0x7F,0x08,0x04,0x04,0x78}, /* h */
    {0x00,0x44,0x7D,0x40,0x00}, /* i */
    {0x20,0x40,0x44,0x3D,0x00}, /* j */
    {0x7F,0x10,0x28,0x44,0x00}, /* k */
    {0x00,0x41,0x7F,0x40,0x00}, /* l */
    {0x7C,0x04,0x18,0x04,0x78}, /* m */
    {0x7C,0x08,0x04,0x04,0x78}, /* n */
    {0x38,0x44,0x44,0x44,0x38}, /* o */
    {0x7C,0x14,0x14,0x14,0x08}, /* p */
    {0x08,0x14,0x14,0x18,0x7C}, /* q */
    {0x7C,0x08,0x04,0x04,0x08}, /* r */
    {0x48,0x54,0x54,0x54,0x20}, /* s */
    {0x04,0x3F,0x44,0x40,0x20}, /* t */
    {0x3C,0x40,0x40,0x20,0x7C}, /* u */
    {0x1C,0x20,0x40,0x20,0x1C}, /* v */
    {0x3C,0x40,0x30,0x40,0x3C}, /* w */
    {0x44,0x28,0x10,0x28,0x44}, /* x */
    {0x0C,0x50,0x50,0x50,0x3C}, /* y */
    {0x44,0x64,0x54,0x4C,0x44}, /* z */
    {0x00,0x08,0x36,0x41,0x00}, /* { */
    {0x00,0x00,0x7F,0x00,0x00}, /* | */
    {0x00,0x41,0x36,0x08,0x00}, /* } */
    {0x08,0x04,0x08,0x10,0x08}, /* ~ */
};

#define FONT_W 5
#define FONT_H 7
#define FONT_ADVANCE 6   // one pixel of spacing between glyphs
#define FONT_FIRST ' '
#define FONT_COUNT 95

// The transposed glyph handed back to gfx. gfx requires the returned pointer to
// stay valid until the next call for the same font, which a single static
// buffer satisfies because the library draws each glyph before asking for the
// next one.
static uint8_t scratch[((FONT_W + 7) / 8) * FONT_H];

static const void *font_glyph(const gfx_font_t *font, uint32_t codepoint,
                              int *advance) {
    (void)font;
    *advance = FONT_ADVANCE;

    if (codepoint < (uint32_t)FONT_FIRST || codepoint >= FONT_COUNT + FONT_FIRST)
        return 0;   // no glyph; gfx draws its fallback box

    const uint8_t *cols = font5x7[codepoint - FONT_FIRST];

    // Column-major to row-major, and pad each row out to a whole byte so the
    // layout matches the 1bpp contract in the gfx header. That contract is
    // MSB-first: gfx reads bit 7 as the leftmost column, so column 0 goes there
    // too. Using bit (FONT_W - 1 - col) instead shifts every glyph right by
    // three columns, which scrambles the text without looking obviously wrong.
    int stride = (FONT_W + 7) / 8;
    for (int row = 0; row < FONT_H; row++) {
        uint8_t bits = 0;
        for (int col = 0; col < FONT_W; col++)
            if (cols[col] & (1u << row))
                bits |= (uint8_t)(1u << (7 - col));
        scratch[row * stride] = bits;
    }
    return scratch;
}

static const gfx_font_t the_font = {
    .glyph_w     = FONT_W,
    .glyph_h     = FONT_H,
    .ascent      = FONT_H,
    .line_height = FONT_H + 2,
    .bpp         = 1,
    .glyph       = font_glyph,
};

const gfx_font_t *ex_font(void) {
    return &the_font;
}

void ex_draw_label(gfx_surface_t *s, const char *text, int x, int y,
                   gfx_color_t c) {
    // gfx_text places text by its baseline, but callers almost always want to
    // think in terms of the top-left corner, so shift down by the ascent.
    gfx_text(s, ex_font(), x, y + FONT_H, text, c);
}

void ex_draw_label_centered(gfx_surface_t *s, const char *text, int x, int w,
                            int y, gfx_color_t c) {
    int tw = gfx_text_width(ex_font(), text);
    ex_draw_label(s, text, x + (w - tw) / 2, y, c);
}
