#pragma once

#include <stddef.h>
#include <stdint.h>
#include "gui_protocol.h"

// The cursor is an ordinary window as far as the compositor is concerned: it
// lives in the same window table, gets composited like everything else, and is
// simply forced to the top of the z-order and skipped by hit-testing. Keeping it
// in the table rather than special-casing it at present time means a client can
// later replace the sprite by drawing into the cursor window's own region.

// Sprite shape, 16 rows x 16 columns, MSB (bit 15) = leftmost column:
//   . transparent   # white fill   X black outline
//   .XX..............
//   .X#X.............
//   .X##X............
//   .X###X...........
//   .X####X..........
//   .X#####X.........
//   .X#####X.........
//   .X######X........
//   .X#######X.......
//   .X######XX.......
//   .X#####X.........
//   .X####X...........
//   .X###X...........
//   .X#XX.............
//   .XX...............
//   .X...............
// Two masks per row rather than one packed value: the outline is easy to read
// against the fill this way, and a 16-bit mask is what a 16-pixel-wide row needs
// (a uint8_t can only describe the left 8 columns).
static const uint16_t gui_cursor_on[GUI_CURSOR_H] = {
    0xC000, 0xE000, 0xF000, 0xF800,
    0xFC00, 0xFE00, 0xFE00, 0xFF00,
    0xFF80, 0xFF80, 0xFE00, 0xFC00,
    0xF800, 0xF000, 0xC000, 0x8000,
};

// Subset of gui_cursor_on that is drawn in the outline colour
static const uint16_t gui_cursor_black[GUI_CURSOR_H] = {
    0xC000, 0xA000, 0x9000, 0x8800,
    0x8400, 0x8200, 0x8200, 0x8100,
    0x8080, 0x8180, 0x8200, 0x8400,
    0x8800, 0xB000, 0xC000, 0x8000,
};

// Colours matching the sprite's palette indices. The top byte is real alpha: 0x00
// marks a transparent pixel, so the cursor can be composited over whatever is
// beneath it instead of stamping an opaque black box.
#define GUI_CURSOR_TRANSPARENT 0x00000000u
#define GUI_CURSOR_WHITE       0xFFFFFFFFu
#define GUI_CURSOR_BLACK       0xFF000000u

// Rasterise the sprite into a GUI_CURSOR_W by GUI_CURSOR_H ARGB window buffer.
static inline void gui_cursor_blit(uint32_t *dst, int dst_w, int at_x, int at_y) {
    for (int row = 0; row < GUI_CURSOR_H; row++) {
        uint16_t on = gui_cursor_on[row];
        uint16_t black = gui_cursor_black[row];
        for (int col = 0; col < GUI_CURSOR_W; col++) {
            int px = at_x + col, py = at_y + row;
            if (px < 0 || py < 0 || px >= dst_w) continue;
            uint16_t bit = (uint16_t)(1u << (15 - col));
            uint32_t c = GUI_CURSOR_TRANSPARENT;
            if (on & bit) c = (black & bit) ? GUI_CURSOR_BLACK : GUI_CURSOR_WHITE;
            dst[(size_t)py * dst_w + px] = c;
        }
    }
}
