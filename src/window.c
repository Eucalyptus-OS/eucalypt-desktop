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
