// This is my first attempt at writing a window manager.

#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>

struct window {
    int x;
    int y;
    int width;
    int height;
    int z_index;
    bool focused;
    bool visible;
    struct window *next;
};

static struct window *windows = NULL;

// Initializing the window manager
int init_manager() {
    // We must end the shell process before we can start the window manager
    if (system("killall -9 shell") != 0) {
        fprintf(stderr, "Failed to kill shell process\n");
        return -1;
    }

    // Clear the screen
    printf("\033[2J\033[H");

    windows = malloc(sizeof(struct window));
    if (!windows) {
        fprintf(stderr, "Failed to allocate memory for windows\n");
        return -1;
    }
    windows->next = NULL;
    return 0;
}
