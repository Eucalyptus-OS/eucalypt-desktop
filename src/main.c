#include "window.h"
#include "fb.h"
#include <stdint.h>

extern void set_background(uint32_t color);

int main() {
    if (init_manager() != 0) {
        return -1;
    }

    // Must come before any drawing: without it there is no shadow buffer and
    // fb_clear()/fb_present() silently do nothing.
    if (fb_init() != 0) {
        return -1;
    }

    set_background(0xFF008080);

    while (1) {

    }

    return 0;
}
