#include "gui/gui_server.h"

#include <stdint.h>

// The desktop is the compositor. It is the only process that opens /dev/fb0;
// every other app is handed a shared-memory window and draws into that instead.
//
// window.c and background.c are still compiled in and are where a window
// manager would eventually live, but the compositor loop has to own the
// framebuffer before anything else can draw.

int main() {
    if (gui_server_init() != 0) {
        return -1;
    }

    gui_server_run();

    return 0;
}
