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
} __attribute__((packed)); // Packed to avoid padding and ensure the struct is the same size in shared memory.

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

// When a windows is created it is handed to an app via shared memory. The app can then set the window's properties and the window manager will handle drawing it to the screen.
struct window *create_window(int x, int y, int width, int height) {
    struct window *new_window = malloc(sizeof(struct window));
    if (!new_window) {
        fprintf(stderr, "Failed to allocate memory for new window\n");
        return NULL;
    }
    new_window->x = x;
    new_window->y = y;
    new_window->width = width;
    new_window->height = height;
    new_window->z_index = 0;
    new_window->focused = false;
    new_window->visible = true;
    new_window->next = windows;
    windows = new_window;
    return new_window;
}