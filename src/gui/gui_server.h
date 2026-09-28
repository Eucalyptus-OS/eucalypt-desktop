#pragma once

// The compositor. This is the only process that opens /dev/fb0. It allocates a
// shared-memory region per window, keeps the z-order, composites everything into
// its shadow buffer, and routes input to whichever window is under the pointer.

int gui_server_init(void);
void gui_server_run(void);

// Exposed for the boot banner; returns the number of live windows.
int gui_server_window_count(void);
